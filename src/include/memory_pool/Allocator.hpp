
#pragma once

#include "memory_pool/SlotStatusRegistry.hpp"

#include <atomic>
#include <bitset>
#include <concepts>
#include <cstddef>
#include <expected>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace mp {

template <typename T>
concept PointerType = std::is_pointer_v<T>;

// TODO Candidate to be removed in case we do not need to create the default objects in
// Allocator::initialize()
template <typename T>
concept Allocatable = std::default_initializable<T>;

template <PointerType TYPE, size_t CAPACITY>
    requires(CAPACITY > 0u)
class Bucket final {
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
    [[nodiscard]] bool push_back(TYPE slot) {
        if (size_ < CAPACITY) {
            data_[size_++] = slot;
            return true;
        }
        return false;
    }

    [[nodiscard]] size_t size() const { return size_; }
    iterator begin() { return iterator{data_}; }
    iterator end() { return iterator{data_ + size_}; }

    auto operator[](size_t idx) -> std::expected<TYPE, error::Result> {
        if (idx >= size_) {
            return error::unexp(error::Code::BucketIndexOutOfBounds,
                                std::format("Bucket::operator[] idx={}", idx));
        }
        return data_[idx];
    }

private:
    TYPE data_[CAPACITY];
    size_t size_ = 0u;
};

template <Allocatable TYPE, size_t CAPACITY>
    requires(CAPACITY > 0u)
class Allocator final {
public:
    using size = TYPE;

    ~Allocator() {
        deinitialize();
        std::free(storage_);
    }

    [[nodiscard("Predicate should be verified")]] constexpr bool isInitialized() const {
        return initialized_.load(std::memory_order_acquire);
    }

    auto initialize() -> std::expected<bool, error::Result> {
        if (isInitialized()) {
            return error::unexp(error::Code::CannotInitializeAgain);
        }
        auto const requiredSize = CAPACITY * sizeof(TYPE);

        if (storage_ = static_cast<TYPE *>(std::aligned_alloc(alignof(TYPE), requiredSize));
            !storage_) {
            return error::unexp(error::Code::UnableToAllocateMemory);
        }
        // TODO: This was the first approach to initialize all the memory with default instance of
        // TYPE, that's why there is a concept 'Allocatable'.
        //
        // for (size_t idx = 0; idx < CAPACITY; ++idx) {
        //     ::new (&storage_[idx]) TYPE{}; // TODO could it raise an exception?
        // }

        initialized_.store(true, std::memory_order_release);
        return true;
    }

    void deinitialize() {
        if (isInitialized()) {
            std::destroy(storage_, storage_ + CAPACITY);
        }
        initialized_.store(false, std::memory_order_release);
        registry_.reset();
    }

    template <typename... ARGS>
    [[nodiscard]] constexpr auto allocate(ARGS &&...args) noexcept
        -> std::expected<TYPE *, error::Result> {
        if (!isInitialized()) {
            return error::unexp(error::Code::NotInitialized);
        }
        if (auto const indexesExp = registry_.fetch(); indexesExp) {
            try {
                // Supposed to have only one index
                size_t const idx = (*indexesExp).at(0u);

                // TODO benchmark this allocation. As alternative this placement new can be done by
                // initialize() with the default constructor, then here we create the object with
                // the appropriate arguments and just move it to the position.
                //
                ::new (&storage_[idx]) TYPE{std::forward<ARGS>(args)...};
                return &storage_[idx];

            } catch (std::out_of_range const &ex) {
                // Trying to get the only one index returned by fetching the registry
                return error::unexp(error::Code::InternalLogicError,
                                    "Unable to access the index zero");
            } catch (...) {
                return error::unexp(error::Code::ConstructorHasThrownException);
            }
        }
        return error::unexp(error::Code::InternalLogicError);
    }

    /**
     * @brief Try to allocate a specific number of types in an array fashion.
     */
    template <size_t SIZE>
        requires(SIZE > 0u)
    [[nodiscard]] constexpr auto allocate() -> std::expected<Bucket<TYPE *, SIZE>, error::Result> {
        if (!isInitialized()) {
            return error::unexp(error::Code::NotInitialized);
        }
        Bucket<TYPE *, SIZE> bucket;

        if (auto freeIndexesExp = registry_.fetch(SIZE); freeIndexesExp) {
            for (auto &&idx : *freeIndexesExp) {
                try {
                    ::new (&storage_[idx]) TYPE{};
                    if (!bucket.push_back(&storage_[idx])) {
                        return error::unexp(error::Code::InternalLogicError,
                                            std::format("Cannot push into bucket index={}", idx));
                    }
                } catch (...) {
                    return error::unexp(error::Code::ConstructorHasThrownException);
                }
            }
        } else {
            return error::unexp(error::Code::NoFreeSpace);
        }
        return bucket;
    }

    auto deallocate(TYPE *allocated) noexcept -> std::expected<bool, error::Result> {
        if (!isInitialized()) {
            return error::unexp(error::Code::NotInitialized);
        }
        for (size_t idx = 0; idx < CAPACITY; ++idx) {
            if (allocated == &storage_[idx]) {
                try {
                    storage_[idx].~TYPE();
                } catch (...) {
                    return error::unexp(error::Code::DestructorHasThrownException);
                }
                storage_[idx] = TYPE{};
                registry_.release(idx);
            }
        }
        return true;
    }

    template <typename BTYPE, size_t BSIZE>
        requires(std::same_as<TYPE, BTYPE>)
    auto deallocate(Bucket<BTYPE, BSIZE> &bucket) noexcept -> std::expected<bool, error::Result> {
        for (auto obj : bucket) {
            if (auto result = deallocate(obj); !result) {
                return result.error();
            }
        }
        return true;
    }

    [[nodiscard]] auto status() const {
        return registry_.status();
    }


private:
    SlotStatusRegistry<CAPACITY> registry_;
    std::atomic_bool initialized_ = false;
    TYPE *storage_ = nullptr;
};

} // namespace mp
