#pragma once
#include <string>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#else
#include <curl/curl.h>
#endif
inline std::string https_get(const std::string& host, const std::string& path) {
    std::string result;
#ifdef _WIN32
    struct Handle { HINTERNET value; ~Handle() { if (value) WinHttpCloseHandle(value); } };
    Handle session{WinHttpOpen(L"Pogoda98/2.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)};
    if (!session.value) throw std::runtime_error("Connection failed");
    WinHttpSetTimeouts(session.value, 4000, 4000, 6000, 8000);
    std::wstring whost(host.begin(), host.end()), wpath(path.begin(), path.end());
    Handle connection{WinHttpConnect(session.value, whost.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0)};
    Handle request{connection.value ? WinHttpOpenRequest(connection.value, L"GET", wpath.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr};
    if (!request.value || !WinHttpSendRequest(request.value, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(request.value, nullptr)) throw std::runtime_error("Upstream unavailable");
    DWORD status = 0, size = sizeof(status);
    WinHttpQueryHeaders(request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
    if (status != 200) throw std::runtime_error("Upstream HTTP error");
    char buffer[8192]; DWORD read = 0;
    do {
        if (!WinHttpReadData(request.value, buffer, sizeof(buffer), &read)) throw std::runtime_error("Incomplete response");
        result.append(buffer, read);
        if (result.size() > 2 * 1024 * 1024) throw std::runtime_error("Response too large");
    } while (read);
#else
    static const int initialized = [] { return curl_global_init(CURL_GLOBAL_DEFAULT); }();
    if (initialized) throw std::runtime_error("HTTP initialization failed");
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("Connection failed");
    std::string url = "https://" + host + path;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 4L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 18L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Pogoda98/2.0");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char* ptr, size_t size, size_t count, void* target) -> size_t {
        auto& text = *static_cast<std::string*>(target); size_t bytes = size * count;
        if (text.size() + bytes > 2 * 1024 * 1024) return 0;
        text.append(ptr, bytes); return bytes;
    });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
    auto code = curl_easy_perform(curl); long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status); curl_easy_cleanup(curl);
    if (code != CURLE_OK || status != 200) throw std::runtime_error("Upstream unavailable");
#endif
    return result;
}
