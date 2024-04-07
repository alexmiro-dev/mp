
#pragma once

#include "types.hpp"

#include <algorithm>
#include <atomic>
#include <climits>
#include <cstddef>
#include <expected>
#include <vector>

namespace mp {

using error::code_e;
using error::result_t;

template <size_t N> class slot_status_registry {
public:
    slot_status_registry() = default;
    slot_status_registry(const slot_status_registry&) = delete;
    slot_status_registry(slot_status_registry&&) = delete;
    slot_status_registry& operator=(const slot_status_registry&) = delete;
    slot_status_registry& operator=(slot_status_registry&&) = delete;

    [[nodiscard]] auto fetch(size_t qty = 1u) -> std::expected<std::vector<size_t>, result_t> {
        if (!has_free_space(qty)) {
            return result_t::unexp({code_e::allocator_is_full});
        }
        unsigned int* it = data_;
        static unsigned int* end = data_ + max_index_;
        std::vector<size_t> free_indexes;
        size_t idx{0u};

        while (it < end) {
            if (!is_in_use(idx)) {
                set(idx);
                free_indexes.push_back(idx);

                if (free_indexes.size() == qty) {
                    break;
                }
            }
            ++idx;
            ++it;
        }
        if (free_indexes.size() != qty) {
            return result_t::unexp({code_e::bad_logic});
        }
        return free_indexes;
    }

    void release(size_t idx) {
        if (idx < max_index_ && is_in_use(idx)) {
            unset(idx);
        }
    }

    void reset() {
        unsigned int* it = data_;
        static const unsigned int* end = data_ + max_index_;

        while (it < end) {
            *it++ = 0u;
        }
    }

    struct status_t {
        size_t used{0u};
        size_t free{0u};
    };

    [[nodiscard]] constexpr status_t status() const {
        status_t s{.used = in_use_, .free = max_index_ - in_use_};
        return s;
    }

private:
    [[nodiscard]] bool has_free_space(size_t total_needed) const {
        return total_needed <= (max_index_ - in_use_.load(std::memory_order_acquire));
    }

    void set(size_t idx) {
        if constexpr (power_of_two_) {
            data_[idx / bits_per_int_] |= (1u << (idx & (bits_per_int_ - 1u)));
        } else {
            data_[idx / bits_per_int_] |= (1u << (idx % bits_per_int_));
        }
        in_use_.fetch_add(1u, std::memory_order_release);
    }

    void unset(size_t idx) {
        if constexpr (power_of_two_) {
            data_[idx / bits_per_int_] &= ~(1u << (idx & (bits_per_int_ - 1u)));
        } else {
            data_[idx / bits_per_int_] &= ~(1u << (idx % bits_per_int_));
        }
        in_use_.fetch_sub(1u, std::memory_order_release);
    }

    // Only used internally no need to do a bound check
    bool is_in_use(size_t idx) const {
        if constexpr (power_of_two_) {
            return (data_[idx / bits_per_int_] & (1u << (idx & (bits_per_int_ - 1u)))) != 0u;
        }
        return (data_[idx / bits_per_int_] & (1u << (idx % bits_per_int_))) != 0u;
    }

    static constexpr size_t bits_per_int_ = sizeof(unsigned int) * CHAR_BIT;
    // If NUM_SLOTS is not an exact multiple of IntBits, it ensures that there's enough space to
    // store the remaining bits by adding IntBits - 1u before dividing.
    static constexpr size_t data_size_ = (N + bits_per_int_ - 1u) / bits_per_int_;

    // If NUM_SLOTS is power of two, so is IntBits.
    static constexpr bool power_of_two_ = (N & (N - 1u)) == 0u;

    static constexpr size_t max_index_ = std::min(data_size_, N);

    unsigned int data_[data_size_] = {0u};

    std::atomic_uint in_use_ = 0u;
};

} // namespace mp

