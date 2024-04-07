
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

using error::Ecode;
using error::Result;

template <typename T>
concept PointerType = std::is_pointer_v<T>;

// TODO Candidate to be removed in case we do not need to create the default objects in
// Allocator::initialize()
template <typename T>
concept Allocatable = std::default_initializable<T>;

/**
 * Reserves memory space on the heap
 */
template <Allocatable ALLOC_T, size_t ALLOC_SIZE>
    requires(ALLOC_SIZE > 0u)
class Allocator final {
private:
    static constexpr auto kRequiredSize = ALLOC_SIZE * sizeof(ALLOC_T);

public:
    template <PointerType BUCKET_T, size_t BUCKET_SIZE>
        requires(BUCKET_SIZE > 0u)
    class Bucket final {
    public:
        class iterator {
        public:
            using iterator_category = std::forward_iterator_tag;
            using pointer = BUCKET_T *;
            using reference = BUCKET_T &;
            using value_type = BUCKET_T;

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

        [[nodiscard]] size_t size() const { return size_; }
        iterator begin() { return iterator{data_}; }
        iterator end() { return iterator{data_ + size_}; }

        /**
         * Random access operator
         */
        auto operator[](size_t idx) -> std::expected<BUCKET_T, Result> {
            if (idx >= size_) {
                return Result::unexp({Ecode::BucketIndexOutOfBounds, std::format("Bucket::operator[] idx={}", idx)});
            }
            return data_[idx];
        }

    private:
        friend class Allocator<ALLOC_T, ALLOC_SIZE>;

        [[nodiscard]] bool push_back(BUCKET_T slot) {
            if (size_ < BUCKET_SIZE) {
                data_[size_++] = slot;
                return true;
            }
            return false;
        }

        BUCKET_T data_[BUCKET_SIZE];
        size_t size_ = 0u;
    };

    ~Allocator() {
        deinitialize();
        std::free(storage_);
    }

    [[nodiscard]] constexpr bool is_initialized() const { return initialized_.load(std::memory_order_acquire); }

    auto initialize() -> std::expected<bool, Result> {
        if (is_initialized()) {
            return Result::unexp({Ecode::CannotInitializeAgain});
        }
        if (storage_ = static_cast<ALLOC_T *>(std::aligned_alloc(alignof(ALLOC_T), kRequiredSize)); !storage_) {
            return Result::unexp({Ecode::UnableToAllocateMemory});
        }
        initialized_.store(true, std::memory_order_release);
        return true;
    }

    void deinitialize() {
        if (is_initialized()) {
            std::destroy(storage_, storage_ + ALLOC_SIZE);
        }
        initialized_.store(false, std::memory_order_release);
        registry_.reset();
    }

    template <typename... ARGS>
    [[nodiscard]] constexpr auto allocate(ARGS &&...args) noexcept -> std::expected<ALLOC_T *, Result> {
        if (!is_initialized()) {
            return Result::unexp({Ecode::NotInitialized});
        }
        if (auto const indexesExp = registry_.fetch(); indexesExp) {
            try {
                // Supposed to have only one index
                size_t const idx = (*indexesExp).at(0u);

                // TODO benchmark this allocation. As alternative this placement new can be done by
                // initialize() with the default constructor, then here we create the object with
                // the appropriate arguments and just move it to the position.
                //
                ::new (&storage_[idx]) ALLOC_T{std::forward<ARGS>(args)...};
                return &storage_[idx];

            } catch (std::out_of_range const &ex) {
                return Result::unexp({Ecode::InternalLogicError, "Unable to access the fetched index"});
            } catch (...) {
                return Result::unexp({Ecode::ConstructorHasThrownException});
            }
        }
        return Result::unexp({Ecode::InternalLogicError});
    }

    /**
     * @brief Try to allocate a specific number of types in an array fashion.
     */
    template <size_t SIZE>
        requires(SIZE > 0u)
    [[nodiscard]] constexpr auto allocate_bucket() -> std::expected<Bucket<ALLOC_T *, SIZE>, Result> {
        if (!is_initialized()) {
            return Result::unexp({Ecode::NotInitialized});
        }
        Bucket<ALLOC_T *, SIZE> bucket;

        if (auto freeIndexesExp = registry_.fetch(SIZE); freeIndexesExp) {
            for (auto &&idx : *freeIndexesExp) {
                try {
                    ::new (&storage_[idx]) ALLOC_T{};
                    if (!bucket.push_back(&storage_[idx])) {
                        return Result::unexp({Ecode::InternalLogicError, std::format("Cannot push into bucket index={}", idx)});
                    }
                } catch (...) {
                    return Result::unexp({Ecode::ConstructorHasThrownException});
                }
            }
        } else {
            return Result::unexp({Ecode::NoFreeSpace});
        }
        return bucket;
    }

    auto deallocate(ALLOC_T *allocated) noexcept -> std::expected<bool, Result> {
        if (!is_initialized()) {
            return Result::unexp({Ecode::NotInitialized});
        }
        for (size_t idx = 0; idx < ALLOC_SIZE; ++idx) {
            if (allocated == &storage_[idx]) {
                try {
                    storage_[idx].~ALLOC_T();
                } catch (...) {
                    return Result::unexp({Ecode::DestructorHasThrownException});
                }
                storage_[idx] = ALLOC_T{};
                registry_.release(idx);
            }
        }
        return true;
    }

    template <typename BTYPE, size_t BSIZE>
        requires(std::same_as<ALLOC_T, BTYPE>)
    auto deallocate(Bucket<BTYPE, BSIZE> &bucket) noexcept -> std::expected<bool, Result> {
        for (auto obj : bucket) {
            if (auto result = deallocate(obj); !result) {
                return result.error();
            }
        }
        return true;
    }

    [[nodiscard]] auto status() const { return registry_.status(); }

private:
    SlotStatusRegistry<ALLOC_SIZE> registry_;
    std::atomic_bool initialized_ = false;
    ALLOC_T *storage_ = nullptr;
};

} // namespace mp
