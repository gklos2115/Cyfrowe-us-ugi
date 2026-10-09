#pragma once
#include "http_client.h"
#include <chrono>
#include <cmath>
inline json upstream(const std::string& host, const std::string& path) {
    struct Entry { json data; std::chrono::steady_clock::time_point time; };
    static std::mutex mutex;
    static std::unordered_map<std::string, Entry> cache;
    const auto key = host + path;
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = cache.find(key);
        if (it != cache.end() && std::chrono::steady_clock::now() - it->second.time < std::chrono::minutes(5)) return it->second.data;
    }
    auto data = json::parse(https_get(host, path));
    if (!data.is_object() || data.contains("error")) throw std::runtime_error("Invalid upstream data");
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (cache.size() >= 128) cache.clear();
        cache[key] = {data, std::chrono::steady_clock::now()};
    }
    return data;
}
inline bool coordinates(const httplib::Request& req, httplib::Response& res, std::string& lat, std::string& lon) {
    try {
        lat = req.get_param_value("lat"); lon = req.get_param_value("lon");
        size_t a, b; double x = std::stod(lat, &a), y = std::stod(lon, &b);
        if (a != lat.size() || b != lon.size() || !std::isfinite(x) || !std::isfinite(y) || std::abs(x) > 90 || std::abs(y) > 180) throw std::invalid_argument("coordinates");
        lat = std::to_string(x); lon = std::to_string(y); return true;
    } catch (...) {
        res.status = 400; res.set_content(R"({"error":"Nieprawidlowe wspolrzedne miejscowosci."})", "application/json"); return false;
    }
}
inline json number(const json& object, const char* key) {
    if (!object.contains(key) || object[key].is_null()) return nullptr;
    if (object[key].is_number()) return object[key];
    if (object[key].is_string()) {
        try { auto s = object[key].get<std::string>(); size_t used; double n = std::stod(s, &used);
            if (used == s.size() && std::isfinite(n)) return n;
        } catch (...) {}
    }
    return nullptr;
}
void setup_api_routes(httplib::Server& svr) {
    svr.Get("/api/locations", [](const httplib::Request& req, httplib::Response& res) {
        if (!check_auth(req, res)) return;
        auto name = req.get_param_value("name");
        if (name.size() < 2 || name.size() > 120) {
            res.status = 400; res.set_content(R"({"error":"Wpisz od 2 do 120 znakow."})", "application/json"); return;
        }
        try {
            auto j = upstream("geocoding-api.open-meteo.com", "/v1/search?count=8&language=pl&format=json&name=" + url_encode(name));
            json out = json::array();
            for (const auto& city : j.value("results", json::array())) {
                out.push_back({{"name", city.at("name")}, {"lat", city.at("latitude")}, {"lon", city.at("longitude")}, {"country", city.value("country", "")}, {"region", city.value("admin1", "")}, {"timezone", city.value("timezone", "UTC")}});
            }
            res.set_content(json({{"results", out}}).dump(), "application/json");
        } catch (...) { res.status = 502; res.set_content(R"({"error":"Wyszukiwarka miejscowosci jest niedostepna. Sprobuj ponownie."})", "application/json"); }
    });
    auto route = [&svr](const std::string& source) {
        svr.Get("/api/weather/" + source, [source](const httplib::Request& req, httplib::Response& res) {
            if (!check_auth(req, res)) return;
            std::string lat, lon;
            if (!coordinates(req, res, lat, lon)) return;
            try {
                json out;
                if (source == "openmeteo") {
                    auto j = upstream("api.open-meteo.com", "/v1/forecast?latitude=" + lat + "&longitude=" + lon + "&current=temperature_2m,apparent_temperature,relative_humidity_2m,wind_speed_10m,weather_code,surface_pressure,is_day&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max&forecast_days=7&timezone=auto");
                    const auto& cur = j.at("current");
                    if (number(cur, "temperature_2m").is_null()) throw std::runtime_error("Missing current temperature");
                    out = {{"source", "Open-Meteo"}, {"temperature_c", number(cur,"temperature_2m")}, {"feels_like_c", number(cur,"apparent_temperature")}, {"humidity_percent", number(cur,"relative_humidity_2m")}, {"wind_speed_kmh", number(cur,"wind_speed_10m")}, {"pressure_mb", number(cur,"surface_pressure")}, {"weather_code", number(cur,"weather_code")}, {"is_day", number(cur,"is_day")}, {"time", cur.value("time", "")}, {"timezone", j.value("timezone", "UTC")}, {"daily", j.value("daily", json::object())}};
                } else if (source == "wttr") {
                    auto j = upstream("wttr.in", "/" + lat + "," + lon + "?format=j1&lang=pl");
                    const auto& cur = j.at("current_condition").at(0);
                    if (number(cur,"temp_C").is_null()) throw std::runtime_error("Missing current temperature");
                    auto descriptions = cur.value("lang_pl", cur.value("weatherDesc", json::array()));
                    out = {{"source", "wttr.in"}, {"temperature_c", number(cur,"temp_C")}, {"feels_like_c", number(cur,"FeelsLikeC")}, {"humidity_percent", number(cur,"humidity")}, {"wind_speed_kmh", number(cur,"windspeedKmph")}, {"pressure_mb", number(cur,"pressure")}, {"description", descriptions.empty() ? "" : descriptions.at(0).value("value", "")}, {"time", cur.value("localObsDateTime", "")}};
                } else {
                    auto j = upstream("www.7timer.info", "/bin/api.pl?lon=" + lon + "&lat=" + lat + "&product=civil&output=json&unit=metric");
                    const auto& series = j.at("dataseries");
                    if (!series.is_array() || series.empty()) throw std::runtime_error("Missing forecast");
                    json forecast = json::array();
                    for (const auto& e : series) forecast.push_back({{"timepoint_h", number(e,"timepoint")}, {"temperature_c", number(e,"temp2m")}, {"weather", e.value("weather", "")}, {"wind_speed_category", number(e.at("wind10m"),"speed")}});
                    out = {{"source", "7Timer"}, {"init_time", j.at("init")}, {"forecast", forecast}};
                }
                out["city"] = req.get_param_value("city"); res.set_content(out.dump(), "application/json");
            } catch (...) {
                res.status = 502; res.set_content(json({{"error", "Zrodlo chwilowo niedostepne. Sprobuj ponownie."}, {"source", source}}).dump(), "application/json");
            }
        });
    };
    route("openmeteo"); route("wttr"); route("7timer");
}
