#include "caps.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

// Stub for allAttributes() — needed by printCapabilities in caps.cpp
std::vector<std::pair<std::string, uint8_t>> allAttributes() {
    return {
        {"luminance", 0x10},
        {"contrast", 0x12},
        {"input", 0x60},
        {"volume", 0x62},
        {"mute", 0x8D},
        {"standby", 0xD6},
        {"input-alt", 0xF4},
    };
}

static int testCount = 0;
static int passCount = 0;

#define TEST(name)                                                    \
    static void test_##name();                                        \
    struct Register_##name {                                          \
        Register_##name() { test_##name(); }                          \
    } register_##name;                                                \
    static void test_##name()

#define ASSERT_EQ(a, b)                                               \
    do {                                                              \
        ++testCount;                                                  \
        if ((a) == (b)) {                                             \
            ++passCount;                                              \
        } else {                                                      \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__       \
                      << ": " #a " != " #b "\n";                     \
        }                                                             \
    } while (0)

#define ASSERT_TRUE(expr)                                             \
    do {                                                              \
        ++testCount;                                                  \
        if ((expr)) {                                                 \
            ++passCount;                                              \
        } else {                                                      \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__       \
                      << ": " #expr " is false\n";                    \
        }                                                             \
    } while (0)

TEST(basic_discrete_values) {
    auto caps = parseCapabilities("(vcp(60(0F 11 12)))");
    ASSERT_EQ(caps.size(), 1u);
    auto it = caps.find(0x60);
    ASSERT_TRUE(it != caps.end());
    ASSERT_EQ(it->second.size(), 3u);
    ASSERT_EQ(it->second[0], 0x0F);
    ASSERT_EQ(it->second[1], 0x11);
    ASSERT_EQ(it->second[2], 0x12);
}

TEST(continuous_values) {
    auto caps = parseCapabilities("(vcp(10 12))");
    ASSERT_EQ(caps.size(), 2u);
    auto it10 = caps.find(0x10);
    ASSERT_TRUE(it10 != caps.end());
    ASSERT_TRUE(it10->second.empty());
    auto it12 = caps.find(0x12);
    ASSERT_TRUE(it12 != caps.end());
    ASSERT_TRUE(it12->second.empty());
}

TEST(mixed_capabilities) {
    // Realistic capabilities string snippet
    auto caps = parseCapabilities(
        "(prot(monitor)type(lcd)model(ABC)"
        "vcp(10 12 60(0F 11 12) 62 8D(01 02) D6(01 04 05) F4(01 02 03 04))"
        ")");

    ASSERT_EQ(caps.size(), 7u);

    // Continuous: 0x10, 0x12, 0x62
    ASSERT_TRUE(caps[0x10].empty());
    ASSERT_TRUE(caps[0x12].empty());
    ASSERT_TRUE(caps[0x62].empty());

    // Discrete: 0x60
    ASSERT_EQ(caps[0x60].size(), 3u);
    ASSERT_EQ(caps[0x60][0], 0x0F);
    ASSERT_EQ(caps[0x60][1], 0x11);
    ASSERT_EQ(caps[0x60][2], 0x12);

    // Discrete: 0x8D
    ASSERT_EQ(caps[0x8D].size(), 2u);
    ASSERT_EQ(caps[0x8D][0], 0x01);
    ASSERT_EQ(caps[0x8D][1], 0x02);

    // Discrete: 0xD6
    ASSERT_EQ(caps[0xD6].size(), 3u);
    ASSERT_EQ(caps[0xD6][0], 0x01);
    ASSERT_EQ(caps[0xD6][1], 0x04);
    ASSERT_EQ(caps[0xD6][2], 0x05);

    // Discrete: 0xF4
    ASSERT_EQ(caps[0xF4].size(), 4u);
}

TEST(empty_vcp_section) {
    auto caps = parseCapabilities("(vcp())");
    ASSERT_TRUE(caps.empty());
}

TEST(empty_string) {
    auto caps = parseCapabilities("");
    ASSERT_TRUE(caps.empty());
}

TEST(no_vcp_section) {
    auto caps = parseCapabilities("(prot(monitor)type(lcd)model(XYZ))");
    ASSERT_TRUE(caps.empty());
}

TEST(case_insensitive_vcp_tag) {
    auto caps = parseCapabilities("(VCP(10 12))");
    ASSERT_EQ(caps.size(), 2u);
    ASSERT_TRUE(caps.count(0x10));
    ASSERT_TRUE(caps.count(0x12));
}

int main() {
    if (passCount == testCount) {
        std::cout << "All " << testCount << " assertions passed.\n";
        return 0;
    }
    std::cerr << passCount << "/" << testCount << " assertions passed.\n";
    return 1;
}
