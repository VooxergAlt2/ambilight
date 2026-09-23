#pragma once

#include <cstddef>
#include <cstdint>

namespace ambilight {

class OtaImageValidator {
public:
    static constexpr std::size_t kRequiredPrefixBytes = 36;
    static constexpr std::uint8_t kImageMagic = 0xE9;
    static constexpr std::uint16_t kEsp32C6ChipId = 0x000D;
    static constexpr std::uint32_t kAppDescMagic = 0xABCD5432UL;
    static constexpr std::uint8_t kMaxSegments = 16;
    static constexpr std::uint32_t kAppDescBytes = 256;

    static bool validApplicationPrefix(
        const std::uint8_t* data,
        std::size_t length) {

        if (data == nullptr ||
            length < kRequiredPrefixBytes) {

            return false;
        }

        if (data[0] != kImageMagic ||
            data[1] == 0 ||
            data[1] > kMaxSegments) {

            return false;
        }

        const std::uint16_t chipId =
            readLe16(
                data + 12);

        if (chipId !=
            kEsp32C6ChipId) {

            return false;
        }

        // esp_image_header_t is 24 bytes, followed by an 8-byte first segment
        // header. ESP-IDF application images place esp_app_desc_t at the start
        // of the first segment; bootloaders/factory images do not satisfy this
        // application descriptor contract.
        const std::uint32_t firstSegmentBytes =
            readLe32(
                data + 28);

        if (firstSegmentBytes <
            kAppDescBytes) {

            return false;
        }

        return
            readLe32(
                data + 32) ==
            kAppDescMagic;
    }

private:
    static std::uint16_t readLe16(
        const std::uint8_t* data) {

        return
            static_cast<std::uint16_t>(
                data[0]) |
            static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(
                    data[1]) <<
                8U);
    }

    static std::uint32_t readLe32(
        const std::uint8_t* data) {

        return
            static_cast<std::uint32_t>(
                data[0]) |
            (
                static_cast<std::uint32_t>(
                    data[1]) <<
                8U
            ) |
            (
                static_cast<std::uint32_t>(
                    data[2]) <<
                16U
            ) |
            (
                static_cast<std::uint32_t>(
                    data[3]) <<
                24U
            );
    }
};

} // namespace ambilight
