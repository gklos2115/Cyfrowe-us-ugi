#pragma once
// ====================================================================
// utils.h - czysta logika (bez zaleznosci od Windows/httplib/sqlite),
// wydzielona z main.cpp, zeby dalo sie ja jednostkowo testowac.
// ====================================================================

#include <cstdint>
#include <string>
#include <random>
#include <sstream>

// ==================== Operacje bitowe ====================

static inline uint32_t rotl32(uint32_t x, int r) {
    r &= 31;
    if (r == 0) return x;
    return (x << r) | (x >> (32 - r));
}

static inline uint32_t rotr32(uint32_t x, int r) {
    r &= 31;
    if (r == 0) return x;
    return (x >> r) | (x << (32 - r));
}

// Wlasny mieszacz bitowy inspirowany FNV-1a + dodatkowe rundy XOR/rotacji.
// Deterministyczny: to samo wejscie zawsze daje to samo wyjscie.
inline uint64_t bitmix_hash(const std::string& input) {
    uint32_t h1 = 0x811C9DC5u;   // FNV offset basis
    uint32_t h2 = 0x9E3779B9u;   // golden ratio constant

    for (unsigned char c : input) {
        h1 ^= c;
        h1 *= 0x01000193u;
        h1 = rotl32(h1, 5);

        h2 = rotl32(h2, 3);
        h2 ^= (h1 & 0xFF00FF00u) | (~h1 & 0x00FF00FFu);
        h2 += c;
    }

    h1 ^= h1 >> 16;
    h1 *= 0x85EBCA6Bu;
    h1 ^= h1 >> 13;

    h2 = rotr32(h2, 7);
    h2 ^= rotl32(h1, 11);

    uint64_t combined = (static_cast<uint64_t>(h1) << 32) | h2;
    combined ^= (combined >> 33);
    return combined;
}

inline std::string sha256_simple(const std::string& input) {
    uint64_t h = bitmix_hash(input + "salt_uslugi_2026");
    std::ostringstream ss;
    ss << std::hex << h;
    return ss.str();
}

// ==================== Generatory (RNG wstrzykiwalny -> testowalne) ====================

inline std::string generate_code(std::mt19937& rng, int len = 6) {
    static const char chars[] = "0123456789";
    std::string code;
    code.reserve(len);
    for (int i = 0; i < len; i++)
        code += chars[rng() % (sizeof(chars) - 1)];
    return code;
}

inline std::string generate_token(std::mt19937& rng) {
    uint32_t a = rng(), b = rng(), c = rng(), d = rng();

    a ^= rotl32(b, 9);
    b ^= rotr32(c, 13) | (d & 0x0F0F0F0Fu);
    c ^= (a << 7) ^ (d >> 3);
    d ^= ~(a & b) | (c ^ 0xA5A5A5A5u);

    std::ostringstream ss;
    ss << std::hex << a << b << c << d;
    return ss.str();
}

// Domyslny globalny RNG uzywany przez serwer w runtime (osobne ziarno na proces).
inline std::mt19937& default_rng() {
    static thread_local std::mt19937 rng(std::random_device{}());
    return rng;
}

inline std::string generate_code(int len = 6) { return generate_code(default_rng(), len); }
inline std::string generate_token() { return generate_token(default_rng()); }

// ==================== Tekst / URL ====================

inline std::string url_encode(const std::string& s) {
    static const char hex_digits[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size() * 3);
    for (unsigned char c : s) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out += static_cast<char>(c);
        } else {
            out += '%';
            out += hex_digits[c >> 4];
            out += hex_digits[c & 0x0F];
        }
    }
    return out;
}

// Wyciaga token z naglowka "Authorization: Bearer <token>".
// Czysta funkcja stringowa - nie wymaga httplib::Request, wiec latwo ja testowac.
inline std::string extract_bearer_token(const std::string& auth_header) {
    const std::string prefix = "Bearer ";
    if (auth_header.compare(0, prefix.size(), prefix) == 0)
        return auth_header.substr(prefix.size());
    return "";
}
