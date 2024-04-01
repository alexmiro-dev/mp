
#pragma once

#include <atomic>
#include <bitset>
#include <concepts>
#include <cstddef>
#include <expected>
#include <iterator>
#include <memory>
#include <type_traits>

namespace mp {

enum class Error {
    UnexpectedCodePath,
    NotInitialized,
    CannotInitializeAgain,
    UnableToAllocateMemory,
    NoFreeSpace,
    DestructorHasThrownException,
    BucketIndexOutOfBounds
};

template <typename T>
concept PointerType = std::is_pointer_v<T>;

template <typename T>
concept Allocatable = std::default_initializable<T>;

template <PointerType TYPE, size_t CAPACITY>
    requires(CAPACITY > 0U)
class Bucket {
public:
    class iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using pointer = TYPE *;
        using reference = TYPE &;
        using value_type = TYPE;

        explicit iterator(pointer p) : ptr_{p} {}

        reference operator*() const { return *ptr_; }
        pointer operator->() { return ptr_; }

        // prefix increment
        iterator &operator++() {
            ptr_++;
            return *this;
        }
        // postfix increment
        iterator operator++(int) {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        friend bool operator==(iterator const &a, iterator const &b) { return a.ptr_ == b.ptr_; }
        friend bool operator!=(iterator const &a, iterator const &b) { return a.ptr_ != b.ptr_; }

    private:
        pointer ptr_ = nullptr;
    };

    // TODO consider making it 'private' to be used just by 'Allocator'. Maybe this class
    // should be inner class of 'Allocator'
    [[nodiscard]] bool pushBack(TYPE slot) {
        if (size_ < CAPACITY) {
            data_[size_++] = slot;
            return true;
        }
        return false;
    }

    [[nodiscard]] size_t size() const { return size_; }
    iterator begin() { return iterator{data_}; }
    iterator end() { return iterator{data_ + size_}; }

    auto operator[](size_t idx) -> std::expected<TYPE, Error> {
        if (idx >= size_) {
            return std::unexpected(Error::BucketIndexOutOfBounds);
        }
        return data_[idx];
    }

private:
    TYPE data_[CAPACITY];
    size_t size_ = 0U;
};

template <Allocatable TYPE, size_t CAPACITY>
    requires(CAPACITY > 0U)
class Allocator {
public:
    using size = TYPE;

    ~Allocator() {
        deinitialize();
        std::free(storage_);
    }

    [[nodiscard("Predicate should be verified")]] bool isInitialized() const {
        return initialized_.load(std::memory_order_acquire);
    }

    auto initialize() -> std::expected<bool, Error> {
        if (isInitialized()) {
            return std::unexpected(Error::CannotInitializeAgain);
        }
        auto const requiredSize = CAPACITY * sizeof(TYPE);

        if (storage_ = static_cast<TYPE *>(std::aligned_alloc(alignof(TYPE), requiredSize));
            !storage_) {
            return std::unexpected(Error::UnableToAllocateMemory);
        }
        for (size_t idx = 0; idx < CAPACITY; ++idx) {
            ::new (&storage_[idx]) TYPE{}; // TODO could it raise an exception?
        }
        initialized_.store(true, std::memory_order_release);
        return true;
    }

    void deinitialize() {
        if (isInitialized()) {
            std::destroy(storage_, storage_ + CAPACITY);
        }
        initialized_.store(false, std::memory_order_release);
        slots_.reset();
    }

    [[nodiscard]] constexpr auto allocate() -> std::expected<TYPE *, Error> {
        if (!isInitialized()) {
            return std::unexpected(Error::NotInitialized);
        }
        if (slots_.all()) {
            return std::unexpected(Error::NoFreeSpace);
        }
        for (size_t idx = 0; idx < CAPACITY; ++idx) {
            if (!slots_.test(idx)) {
                slots_.set(idx);
                return &storage_[idx];
            }
        }
        return std::unexpected(Error::UnexpectedCodePath);
    }

    template <size_t SIZE>
        requires(SIZE > 0U)
    [[nodiscard]] constexpr auto allocate() -> std::expected<Bucket<TYPE *, SIZE>, Error> {
        auto x = slots_.to_string();

        if (!isInitialized()) {
            return std::unexpected(Error::NotInitialized);
        }
        if (slots_.all()) {
            return std::unexpected(Error::NoFreeSpace);
        }
        std::bitset<CAPACITY> mask;
        for (auto bit = 0U; bit < SIZE; ++bit) {
            mask.set(bit, true);
        }
        auto y = mask.to_string();
        Bucket<TYPE *, SIZE> bucket;
        size_t rotation{0U};

        while (rotation < CAPACITY) {
            if ((mask & slots_).none()) { // Found enough space.
                for (auto idx = 0U; idx < CAPACITY; ++idx) {
                    if (mask.test(idx)) {
                        if (!bucket.pushBack(&storage_[idx])) {
                            return std::unexpected(Error::UnexpectedCodePath);
                        }
                    }
                }
                break;
            }
            //                          saving the right most bit
            //                  --------^---------------
            mask = mask >> 1U | (mask << (CAPACITY - 1)); // Rotation.
            ++rotation;
        }
        return bucket;
    }

    auto deallocate(TYPE *allocated) noexcept -> std::expected<bool, Error> {
        if (!isInitialized()) {
            return std::unexpected(Error::NotInitialized);
        }
        for (size_t idx = 0; idx < CAPACITY; ++idx) {
            if (allocated == &storage_[idx]) {
                try {
                    storage_[idx].~TYPE();
                } catch (...) {
                    return std::unexpected(Error::DestructorHasThrownException);
                }
                storage_[idx] = TYPE{};
                slots_.reset(idx);
            }
        }
        return true;
    }

    struct Status {
        size_t used{0U};
        size_t remaing{0U};
    };

    [[nodiscard]] constexpr Status status() const {
        Status s;
        s.used = slots_.count();
        s.remaing = CAPACITY - s.used;
        return s;
    }

private:
    // 0 indicates the slot is free, and 1 means it is being used
    std::bitset<CAPACITY> slots_;
    std::atomic_bool initialized_ = false;
    TYPE *storage_ = nullptr;
};

} // namespace mp
