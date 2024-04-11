
#pragma once

#include "slot_status_registry.hpp"

#include <atomic>
#include <concepts>
#include <cstddef>
#include <expected>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace mp {

using error::code_e;
using error::result_t;

template <typename T>
concept PointerType = std::is_pointer_v<T>;

/** This concept is needed because of allcator::allocate_bucket() that initialize the memory by using the
 * default constructor.
 */
template <typename T>
concept Allocatable = std::is_default_constructible_v<T>;

/**
 * Reserves memory space on the heap
 */
template <Allocatable TAlloc, size_t NAlloc>
    requires(NAlloc > 0u)
class allocator final {
public:
    template <PointerType TBucket, size_t NBucket>
        requires(NBucket > 0u)
    class bucket final {
    public:
        class iterator {
        public:
            using iterator_category = std::forward_iterator_tag;
            using pointer = TBucket*;
            using reference = TBucket&;
            using value_type = TBucket;

            explicit iterator(pointer p) : ptr_{p} {}

            reference operator*() const { return *ptr_; }
            pointer operator->() { return ptr_; }

            // prefix increment
            iterator& operator++() {
                ptr_++;
                return *this;
            }
            // postfix increment
            iterator operator++(int) {
                iterator tmp = *this;
                ++(*this);
                return tmp;
            }

            friend bool operator==(const iterator& a, const iterator& b) { return a.ptr_ == b.ptr_; }
            friend bool operator!=(const iterator& a, const iterator& b) { return a.ptr_ != b.ptr_; }

        private:
            pointer ptr_ = nullptr;
        };

        [[nodiscard]] size_t size() const { return size_; }
        iterator begin() { return iterator{data_}; }
        iterator end() { return iterator{data_ + size_}; }

        auto operator[](size_t idx) -> std::expected<TBucket, result_t> {
            if (idx >= size_) {
                return result_t::unexp({code_e::out_of_bounds, std::format("bucket::operator[] idx={}", idx)});
            }
            return data_[idx];
        }

    private:
        friend class allocator<TAlloc, NAlloc>;

        [[nodiscard]] bool push_back(TBucket slot) {
            if (size_ < NBucket) {
                data_[size_++] = slot;
                return true;
            }
            return false;
        }

        TBucket data_[NBucket];
        size_t size_ = 0u;
    };

    ~allocator() {
        deinitialize();
        std::free(storage_);
    }

    [[nodiscard]] constexpr bool is_initialized() const { return initialized_.load(std::memory_order_acquire); }

    auto initialize() -> std::expected<bool, result_t> {
        if (is_initialized()) {
            return result_t::unexp({code_e::already_initialized});
        }
        if (storage_ = static_cast<TAlloc*>(std::aligned_alloc(alignof(TAlloc), required_size_)); !storage_) {
            return result_t::unexp({code_e::cannot_reserve_system_memory});
        }
        initialized_.store(true, std::memory_order_release);
        return true;
    }

    void deinitialize() {
        if (is_initialized()) {
            std::destroy(storage_, storage_ + NAlloc);
        }
        initialized_.store(false, std::memory_order_release);
        registry_.reset();
    }

    template <typename... TArgs>
    [[nodiscard]] constexpr auto allocate(TArgs&&... args) noexcept -> std::expected<TAlloc*, result_t> {
        if (!is_initialized()) {
            return result_t::unexp({code_e::not_initialized});
        }
        if (const auto indexes = registry_.fetch(); indexes) {
            try {
                // Supposed to have only one index
                const size_t first_free = (*indexes).at(0u);

                // TODO benchmark this allocation. As alternative this placement new can be done by
                // initialize() with the default constructor, then here we create the object with
                // the appropriate arguments and just move it to the position.
                //
                ::new (&storage_[first_free]) TAlloc{std::forward<TArgs>(args)...};
                return &storage_[first_free];

            } catch (const std::out_of_range& ex) {
                return result_t::unexp({code_e::bad_logic, "Unable to access the fetched index"});
            } catch (...) {
                return result_t::unexp({code_e::exception_caught_in_ctor});
            }
        }
        return result_t::unexp({code_e::bad_logic});
    }

    /**
     * @brief Try to allocate a specific number of elements in an array fashion. Calls the default constructor
     * of TAlloc to initialize the memory.
     */
    template <size_t SIZE>
        requires(SIZE > 0u)
    [[nodiscard]] constexpr auto allocate_bucket() noexcept -> std::expected<bucket<TAlloc*, NAlloc>, result_t> {
        if (!is_initialized()) {
            return result_t::unexp({code_e::not_initialized});
        }
        bucket<TAlloc*, NAlloc> bucket;

        if (auto frees = registry_.fetch(NAlloc); frees) {
            for (auto&& i : *frees) {
                try {
                    ::new (&storage_[i]) TAlloc{};
                    if (!bucket.push_back(&storage_[i])) {
                        return result_t::unexp({code_e::bad_logic, std::format("Cannot push into bucket index={}", i)});
                    }
                } catch (...) {
                    return result_t::unexp({code_e::exception_caught_in_ctor});
                }
            }
        } else {
            return result_t::unexp({code_e::not_enough_space_in_allocator});
        }
        return bucket;
    }

    auto deallocate(TAlloc* allocated) noexcept -> std::expected<bool, result_t> {
        if (!is_initialized()) {
            return result_t::unexp({code_e::not_initialized});
        }
        for (size_t i = 0; i < NAlloc; ++i) {
            if (allocated == &storage_[i]) {
                try {
                    storage_[i].~TAlloc();
                } catch (...) {
                    return result_t::unexp({code_e::exception_caught_in_dctor});
                }
                storage_[i] = TAlloc{};
                registry_.release(i);
            }
        }
        return true;
    }

    template <typename TBucket, size_t NBucket>
        requires(std::same_as<TAlloc, TBucket>)
    auto deallocate(bucket<TBucket, NBucket>& bucket) noexcept -> std::expected<bool, result_t> {
        for (auto ptr : bucket) {
            if (auto result = deallocate(ptr); !result) {
                return result.error();
            }
        }
        return true;
    }

    [[nodiscard]] auto status() const { return registry_.status(); }

private:
    static constexpr auto required_size_ = NAlloc * sizeof(TAlloc);
    slot_status_registry<NAlloc> registry_;
    std::atomic_bool initialized_ = false;
    TAlloc* storage_ = nullptr;
};

} // namespace mp
