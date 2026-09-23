#include <array>
#include <cstdint>

#include <unity.h>

#include "network/OtaImageValidator.h"

using ambilight::OtaImageValidator;

std::array<std::uint8_t, 64> validPrefix() {
    std::array<std::uint8_t, 64> data{};
    data[0] = OtaImageValidator::kImageMagic;
    data[1] = 3;
    data[12] = 0x0D;
    data[13] = 0x00;
    data[28] = 0x00;
    data[29] = 0x01;
    data[30] = 0x00;
    data[31] = 0x00;
    data[32] = 0x32;
    data[33] = 0x54;
    data[34] = 0xCD;
    data[35] = 0xAB;
    return data;
}

void test_accepts_esp32c6_application_prefix() {
    const auto data = validPrefix();
    TEST_ASSERT_TRUE(
        OtaImageValidator::validApplicationPrefix(
            data.data(),
            data.size()));
}

void test_rejects_truncated_prefix() {
    const auto data = validPrefix();
    TEST_ASSERT_FALSE(
        OtaImageValidator::validApplicationPrefix(
            data.data(),
            OtaImageValidator::kRequiredPrefixBytes - 1));
}

void test_rejects_wrong_chip() {
    auto data = validPrefix();
    data[12] = 0x05;
    TEST_ASSERT_FALSE(
        OtaImageValidator::validApplicationPrefix(
            data.data(),
            data.size()));
}

void test_rejects_bootloader_or_factory_prefix_without_app_descriptor() {
    auto data = validPrefix();
    data[32] = 0;
    data[33] = 0;
    data[34] = 0;
    data[35] = 0;
    TEST_ASSERT_FALSE(
        OtaImageValidator::validApplicationPrefix(
            data.data(),
            data.size()));
}

void test_rejects_first_segment_too_small_for_app_descriptor() {
    auto data = validPrefix();
    data[28] = 0x20;
    data[29] = 0x00;
    TEST_ASSERT_FALSE(
        OtaImageValidator::validApplicationPrefix(
            data.data(),
            data.size()));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_accepts_esp32c6_application_prefix);
    RUN_TEST(test_rejects_truncated_prefix);
    RUN_TEST(test_rejects_wrong_chip);
    RUN_TEST(test_rejects_bootloader_or_factory_prefix_without_app_descriptor);
    RUN_TEST(test_rejects_first_segment_too_small_for_app_descriptor);
    return UNITY_END();
}
