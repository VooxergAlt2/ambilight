#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <utility>

namespace ambilight {

// Small no-throw heap buffer for trivially-copyable runtime data.
//
// Topology changes are rare control-plane operations. They may resize these
// buffers, while realtime render/DDP hot paths only touch existing storage and
// perform no allocation. Allocation failure leaves existing storage unchanged.
template<typename T>
class HeapBuffer {
    static_assert(
        std::is_trivially_copyable<T>::value,
        "HeapBuffer is intentionally limited to trivially-copyable data");

public:
    HeapBuffer() = default;

    explicit HeapBuffer(
        std::size_t count,
        const T& initial = T{}) {

        resize(count, initial);
    }

    HeapBuffer(
        const HeapBuffer& other) {

        assign(other);
    }

    HeapBuffer(
        HeapBuffer&& other) noexcept {

        swap(other);
    }

    ~HeapBuffer() {
        std::free(data_);
    }

    HeapBuffer& operator=(
        const HeapBuffer& other) {

        if (this != &other) {
            // assign() is transactional when growth requires allocation: on
            // failure this buffer remains untouched.
            assign(other);
        }

        return *this;
    }

    HeapBuffer& operator=(
        HeapBuffer&& other) noexcept {

        if (this != &other) {
            clearStorage();
            swap(other);
        }

        return *this;
    }

    bool resize(
        std::size_t count,
        const T& initial = T{}) {

        if (count == size_) {
            return true;
        }

        if (count == 0) {
            size_ = 0;
            return true;
        }

        if (count <= capacity_) {
            for (std::size_t index = size_;
                 index < count;
                 ++index) {

                data_[index] = initial;
            }

            size_ = count;
            return true;
        }

        if (count >
            static_cast<std::size_t>(-1) /
                sizeof(T)) {

            return false;
        }

        T* replacement =
            static_cast<T*>(
                std::malloc(
                    count * sizeof(T)));

        if (replacement == nullptr) {
            return false;
        }

        const std::size_t copied =
            std::min(
                size_,
                count);

        if (copied > 0) {
            std::memcpy(
                replacement,
                data_,
                copied * sizeof(T));
        }

        for (std::size_t index = copied;
             index < count;
             ++index) {

            replacement[index] = initial;
        }

        std::free(data_);
        data_ = replacement;
        size_ = count;
        capacity_ = count;
        return true;
    }

    bool assign(
        const HeapBuffer& other) {

        if (!resize(
                other.size_)) {

            return false;
        }

        if (size_ > 0) {
            std::memcpy(
                data_,
                other.data_,
                size_ * sizeof(T));
        }

        return true;
    }

    void fill(
        const T& value) {

        for (std::size_t index = 0;
             index < size_;
             ++index) {

            data_[index] = value;
        }
    }

    void swap(
        HeapBuffer& other) noexcept {

        std::swap(data_, other.data_);
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
    }

    void clearStorage() {
        std::free(data_);
        data_ = nullptr;
        size_ = 0;
        capacity_ = 0;
    }

    T* data() { return data_; }
    const T* data() const { return data_; }

    std::size_t size() const { return size_; }
    std::size_t capacity() const { return capacity_; }
    bool empty() const { return size_ == 0; }

    T* begin() { return data_; }
    T* end() { return data_ + size_; }
    const T* begin() const { return data_; }
    const T* end() const { return data_ + size_; }

    T& operator[](std::size_t index) {
        return data_[index];
    }

    const T& operator[](std::size_t index) const {
        return data_[index];
    }

private:
    T* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t capacity_ = 0;
};

} // namespace ambilight
