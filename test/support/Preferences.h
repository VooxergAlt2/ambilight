#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

class String {
public:
    String() = default;

    String(const char* value)
        : value_(value != nullptr ? value : "") {}

    std::size_t length() const {
        return value_.size();
    }

    const char* c_str() const {
        return value_.c_str();
    }

private:
    std::string value_;
};

class Preferences {
public:
    bool begin(
        const char*,
        bool = false,
        const char* = nullptr) {

        opened_ = beginResult_;
        return opened_;
    }

    void end() {
        opened_ = false;
    }

    bool clear() {
        if (!opened_ || failClear_) {
            return false;
        }

        storage_.clear();
        return true;
    }

    bool remove(const char* key) {
        if (!opened_ || key == nullptr) {
            return false;
        }

        if (!failRemoveKey_.empty() &&
            failRemoveKey_ == key) {

            return false;
        }

        storage_.erase(key);
        return true;
    }

    bool isKey(const char* key) const {
        return
            opened_ &&
            key != nullptr &&
            storage_.find(key) !=
                storage_.end();
    }

    std::size_t putUChar(
        const char* key,
        std::uint8_t value) {

        return putScalar(
            key,
            value);
    }

    std::uint8_t getUChar(
        const char* key,
        std::uint8_t defaultValue = 0) const {

        return getScalar(
            key,
            defaultValue);
    }

    std::size_t putUShort(
        const char* key,
        std::uint16_t value) {

        return putScalar(
            key,
            value);
    }

    std::uint16_t getUShort(
        const char* key,
        std::uint16_t defaultValue = 0) const {

        return getScalar(
            key,
            defaultValue);
    }

    std::size_t putBytes(
        const char* key,
        const void* data,
        std::size_t length) {

        if (!opened_ ||
            key == nullptr ||
            data == nullptr ||
            (
                !failPutKey_.empty() &&
                failPutKey_ == key
            )) {

            return 0;
        }

        const auto* bytes =
            static_cast<
                const std::uint8_t*>(
                    data);

        storage_[key] =
            std::vector<std::uint8_t>(
                bytes,
                bytes + length);

        return length;
    }

    std::size_t getBytesLength(
        const char* key) const {

        if (!opened_ ||
            key == nullptr) {

            return 0;
        }

        const auto found =
            storage_.find(
                key);

        return
            found == storage_.end()
                ? 0
                : found->second.size();
    }

    std::size_t getBytes(
        const char* key,
        void* destination,
        std::size_t maxLength) const {

        if (!opened_ ||
            key == nullptr ||
            destination == nullptr) {

            return 0;
        }

        const auto found =
            storage_.find(
                key);

        if (found ==
            storage_.end()) {

            return 0;
        }

        const std::size_t length =
            found->second.size();

        if (length >
            maxLength) {

            return 0;
        }

        std::memcpy(
            destination,
            found->second.data(),
            length);

        return length;
    }

    std::size_t putString(
        const char* key,
        const char* value) {

        if (!opened_ ||
            key == nullptr ||
            value == nullptr) {

            return 0;
        }

        const std::size_t length =
            std::strlen(value);

        const auto* bytes =
            reinterpret_cast<
                const std::uint8_t*>(
                    value);

        storage_[key] =
            std::vector<std::uint8_t>(
                bytes,
                bytes + length);

        return length;
    }

    String getString(
        const char* key,
        const String& defaultValue =
            String()) const {

        if (!opened_ ||
            key == nullptr) {

            return defaultValue;
        }

        const auto found =
            storage_.find(
                key);

        if (found ==
            storage_.end()) {

            return defaultValue;
        }

        const std::string value(
            found->second.begin(),
            found->second.end());

        return String(
            value.c_str());
    }

    static void testReset() {
        storage_.clear();
        beginResult_ = true;
        failClear_ = false;
        failRemoveKey_.clear();
        failPutKey_.clear();
    }

    static void testSetBeginResult(
        bool result) {

        beginResult_ = result;
    }

    static void testFailClear(
        bool fail) {

        failClear_ = fail;
    }

    static void testFailRemove(
        const char* key) {

        failRemoveKey_ =
            key != nullptr
                ? key
                : "";
    }

    static void testFailPut(
        const char* key) {

        failPutKey_ =
            key != nullptr
                ? key
                : "";
    }

    static bool testHasKey(
        const char* key) {

        return
            key != nullptr &&
            storage_.find(key) !=
                storage_.end();
    }

private:
    template <typename T>
    std::size_t putScalar(
        const char* key,
        T value) {

        return putBytes(
            key,
            &value,
            sizeof(value));
    }

    template <typename T>
    T getScalar(
        const char* key,
        T defaultValue) const {

        T value{};

        return
            getBytes(
                key,
                &value,
                sizeof(value)) ==
                    sizeof(value)
                ? value
                : defaultValue;
    }

    bool opened_ = false;

    inline static std::unordered_map<
        std::string,
        std::vector<std::uint8_t>>
        storage_{};

    inline static bool beginResult_ = true;
    inline static bool failClear_ = false;
    inline static std::string failRemoveKey_{};
    inline static std::string failPutKey_{};
};
