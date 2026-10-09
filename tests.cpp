// ====================================================================
// tests.cpp - testy jednostkowe dla utils.h
// Lekki wlasny harness (brak gtest offline). Kompilacja:
//   g++ -std=c++17 -O2 -I . tests.cpp -o tests.exe
// Uruchomienie:
//   ./tests.exe
// ====================================================================

#include "utils.h"
#include <iostream>
#include <cassert>
#include <set>
#include <vector>

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << "  " << #cond << "\n"; } \
} while (0)

#define CHECK_EQ(a, b) do { \
    auto va = (a); auto vb = (b); \
    if (va == vb) { g_pass++; } \
    else { g_fail++; std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ \
        << "  " << #a << " == " << #b << "  (got '" << va << "' vs '" << vb << "')\n"; } \
} while (0)

// ---------------------- rotl32 / rotr32 ----------------------

static void test_rotl32() {
    CHECK_EQ(rotl32(0x00000001u, 1), 0x00000002u);
    CHECK_EQ(rotl32(0x80000000u, 1), 0x00000001u);      // zawijanie bitu najwyzszego
    CHECK_EQ(rotl32(0x12345678u, 0), 0x12345678u);       // obrot o 0 = bez zmian
    CHECK_EQ(rotl32(0x12345678u, 32), 0x12345678u);      // 32 mod 32 == 0
    CHECK_EQ(rotl32(0xFFFFFFFFu, 5), 0xFFFFFFFFu);       // same jedynki -> bez zmian
    CHECK_EQ(rotl32(0x00000001u, 31), 0x80000000u);
}

static void test_rotr32() {
    CHECK_EQ(rotr32(0x00000002u, 1), 0x00000001u);
    CHECK_EQ(rotr32(0x00000001u, 1), 0x80000000u);       // zawijanie bitu najnizszego
    CHECK_EQ(rotr32(0x12345678u, 0), 0x12345678u);
    CHECK_EQ(rotr32(0x12345678u, 32), 0x12345678u);
    CHECK_EQ(rotr32(0xFFFFFFFFu, 7), 0xFFFFFFFFu);
}

static void test_rotl_rotr_are_inverses() {
    uint32_t values[] = {0, 1, 0xDEADBEEFu, 0xFFFFFFFFu, 0x80000001u, 12345u};
    for (uint32_t v : values) {
        for (int r = 0; r < 32; r++) {
            CHECK_EQ(rotr32(rotl32(v, r), r), v);
        }
    }
}

// ---------------------- bitmix_hash / sha256_simple ----------------------

static void test_bitmix_hash_deterministic() {
    CHECK_EQ(bitmix_hash("abc"), bitmix_hash("abc"));
    CHECK_EQ(bitmix_hash(""), bitmix_hash(""));
    CHECK_EQ(bitmix_hash("Zaq1@Wsx"), bitmix_hash("Zaq1@Wsx"));
}

static void test_bitmix_hash_avalanche_like() {
    // Rozne wejscia -> (prawie zawsze) rozne hashe.
    CHECK(bitmix_hash("abc") != bitmix_hash("abd"));
    CHECK(bitmix_hash("password") != bitmix_hash("Password"));
    CHECK(bitmix_hash("hello") != bitmix_hash("olleh"));
    CHECK(bitmix_hash("1") != bitmix_hash("2"));
}

static void test_bitmix_hash_no_trivial_collisions_in_small_set() {
    std::set<uint64_t> seen;
    for (int i = 0; i < 2000; i++) {
        seen.insert(bitmix_hash("user" + std::to_string(i)));
    }
    CHECK_EQ(seen.size(), 2000u); // brak kolizji na tej probce
}

static void test_sha256_simple_deterministic_and_salted() {
    CHECK_EQ(sha256_simple("haslo123"), sha256_simple("haslo123"));
    // sol jest doklejana wewnatrz, wiec wynik != bitmix_hash surowego wejscia
    std::ostringstream raw;
    raw << std::hex << bitmix_hash("haslo123");
    CHECK(sha256_simple("haslo123") != raw.str());
}

static void test_sha256_simple_different_passwords_differ() {
    CHECK(sha256_simple("haslo123") != sha256_simple("haslo124"));
    CHECK(sha256_simple("") != sha256_simple("a"));
}

// ---------------------- generate_code ----------------------

static void test_generate_code_length_and_digits() {
    std::mt19937 rng(42);
    for (int trial = 0; trial < 50; trial++) {
        std::string code = generate_code(rng, 6);
        CHECK_EQ(code.size(), 6u);
        for (char c : code) CHECK(c >= '0' && c <= '9');
    }
}

static void test_generate_code_custom_length() {
    std::mt19937 rng(1);
    CHECK_EQ(generate_code(rng, 0).size(), 0u);
    CHECK_EQ(generate_code(rng, 10).size(), 10u);
}

static void test_generate_code_deterministic_with_seeded_rng() {
    std::mt19937 rng1(123);
    std::mt19937 rng2(123);
    CHECK_EQ(generate_code(rng1, 6), generate_code(rng2, 6));
}

static void test_generate_code_varies_across_calls() {
    std::mt19937 rng(7);
    std::string c1 = generate_code(rng, 6);
    std::string c2 = generate_code(rng, 6);
    CHECK(c1 != c2); // kolejne wywolania na tym samym rng powinny (prawie zawsze) sie roznic
}

// ---------------------- generate_token ----------------------

static void test_generate_token_deterministic_with_seeded_rng() {
    std::mt19937 rng1(999);
    std::mt19937 rng2(999);
    CHECK_EQ(generate_token(rng1), generate_token(rng2));
}

static void test_generate_token_not_empty_and_hex() {
    std::mt19937 rng(5);
    std::string t = generate_token(rng);
    CHECK(!t.empty());
    for (char c : t) {
        bool is_hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        CHECK(is_hex);
    }
}

static void test_generate_token_many_are_unique() {
    std::mt19937 rng(2024);
    std::set<std::string> tokens;
    for (int i = 0; i < 1000; i++) tokens.insert(generate_token(rng));
    CHECK_EQ(tokens.size(), 1000u);
}

// ---------------------- url_encode ----------------------

static void test_url_encode_unreserved_chars_passthrough() {
    CHECK_EQ(url_encode("abcXYZ019-_.~"), std::string("abcXYZ019-_.~"));
}

static void test_url_encode_space_and_special() {
    CHECK_EQ(url_encode("a b"), std::string("a%20b"));
    CHECK_EQ(url_encode("Warszawa/Polska"), std::string("Warszawa%2FPolska"));
    CHECK_EQ(url_encode("?a=1&b=2"), std::string("%3Fa%3D1%26b%3D2"));
}

static void test_url_encode_empty() {
    CHECK_EQ(url_encode(""), std::string(""));
}

static void test_url_encode_utf8_bytes() {
    // "Łódź" w UTF-8 ma bajty spoza ASCII - kazdy powinien zostac zakodowany %XX
    std::string s = "\xc5\x81\xc3\xb3" "d" "\xc5\xba"; // "Łódź" (rozdzielone literaly, by uniknac zlaczenia hex-escape z 'd')
    std::string enc = url_encode(s);
    CHECK(enc.find('%') != std::string::npos);
    CHECK(enc.find(' ') == std::string::npos);
}

// ---------------------- extract_bearer_token ----------------------

static void test_extract_bearer_token_basic() {
    CHECK_EQ(extract_bearer_token("Bearer abc123"), std::string("abc123"));
    CHECK_EQ(extract_bearer_token("Bearer "), std::string(""));
}

static void test_extract_bearer_token_missing_prefix() {
    CHECK_EQ(extract_bearer_token("abc123"), std::string(""));
    CHECK_EQ(extract_bearer_token("bearer abc123"), std::string("")); // wielkosc liter ma znaczenie
    CHECK_EQ(extract_bearer_token(""), std::string(""));
}

static void test_extract_bearer_token_with_special_chars() {
    CHECK_EQ(extract_bearer_token("Bearer a1b2-c3d4_ef56"), std::string("a1b2-c3d4_ef56"));
}

static void test_extract_bearer_token_short_string_no_crash() {
    // stringi krotsze niz "Bearer " nie moga powodowac out-of-range
    CHECK_EQ(extract_bearer_token("B"), std::string(""));
    CHECK_EQ(extract_bearer_token(""), std::string(""));
    CHECK_EQ(extract_bearer_token("Bear"), std::string(""));
}

int main() {
    test_rotl32();
    test_rotr32();
    test_rotl_rotr_are_inverses();

    test_bitmix_hash_deterministic();
    test_bitmix_hash_avalanche_like();
    test_bitmix_hash_no_trivial_collisions_in_small_set();

    test_sha256_simple_deterministic_and_salted();
    test_sha256_simple_different_passwords_differ();

    test_generate_code_length_and_digits();
    test_generate_code_custom_length();
    test_generate_code_deterministic_with_seeded_rng();
    test_generate_code_varies_across_calls();

    test_generate_token_deterministic_with_seeded_rng();
    test_generate_token_not_empty_and_hex();
    test_generate_token_many_are_unique();

    test_url_encode_unreserved_chars_passthrough();
    test_url_encode_space_and_special();
    test_url_encode_empty();
    test_url_encode_utf8_bytes();

    test_extract_bearer_token_basic();
    test_extract_bearer_token_missing_prefix();
    test_extract_bearer_token_with_special_chars();
    test_extract_bearer_token_short_string_no_crash();

    std::cout << "\n" << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail == 0 ? 0 : 1;
}
