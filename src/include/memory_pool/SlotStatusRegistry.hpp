
#pragma once

#include "types.hpp"

#include <algorithm>
#include <atomic>
#include <climits>
#include <cstddef>
#include <expected>
#include <vector>

namespace mp {

using error::Ecode;
using error::Result;

template <size_t NUM_SLOTS> class SlotStatusRegistry {
private:
    static constexpr size_t kBitsPerInt = sizeof(unsigned int) * CHAR_BIT;

    // If NUM_SLOTS is not an exact multiple of IntBits, it ensures that there's enough space to
    // store the remaining bits by adding IntBits - 1u before dividing.
    static constexpr size_t kDataSize = (NUM_SLOTS + kBitsPerInt - 1u) / kBitsPerInt;

    // If NUM_SLOTS is power of two, so is IntBits.
    static constexpr bool kIsPowerOfTwo = (NUM_SLOTS & (NUM_SLOTS - 1u)) == 0u;

    static constexpr size_t kMaxIndex = std::min(kDataSize, NUM_SLOTS);

    unsigned int data_[kDataSize] = {0u};

public:
    SlotStatusRegistry() = default;
    SlotStatusRegistry(const SlotStatusRegistry &) = delete;
    SlotStatusRegistry(SlotStatusRegistry &&) = delete;
    SlotStatusRegistry &operator=(const SlotStatusRegistry &) = delete;
    SlotStatusRegistry &operator=(SlotStatusRegistry &&) = delete;

    [[nodiscard]] auto fetch(size_t num = 1u) -> std::expected<std::vector<size_t>, error::Result> {
        if (!has_space(num)) {
            return Result::unexp({Ecode::NoFreeSpace});
        }
        unsigned int *it = data_;
        static unsigned int *end = data_ + kMaxIndex;
        std::vector<size_t> freeIndexes;
        size_t idx{0u};

        while (it < end) {
            if (!is_in_use(idx)) {
                set(idx);
                freeIndexes.push_back(idx);

                if (freeIndexes.size() == num) {
                    break;
                }
            }
            ++idx;
            ++it;
        }
        if (freeIndexes.size() != num) {
            return Result::unexp({Ecode::InternalLogicError});
        }
        return freeIndexes;
    }

    void release(size_t idx) {
        if (idx < kMaxIndex && is_in_use(idx)) {
            unset(idx);
        }
    }

    void reset() {
        unsigned int *it = data_;
        static unsigned int const *end = data_ + kMaxIndex;

        while (it < end) {
            *it++ = 0u;
        }
    }

    struct Status {
        size_t used{0u};
        size_t free{0u};
    };

    [[nodiscard]] constexpr Status status() const {
        Status s{.used = totalAssigned_, .free = kMaxIndex - totalAssigned_};
        return s;
    }

private:
    [[nodiscard]] bool has_space(size_t totalNeeded) const {
        return totalNeeded <= (kMaxIndex - totalAssigned_.load(std::memory_order_acquire));
    }

    void set(size_t idx) {
        if constexpr (kIsPowerOfTwo) {
            data_[idx / kBitsPerInt] |= (1u << (idx & (kBitsPerInt - 1u)));
        } else {
            data_[idx / kBitsPerInt] |= (1u << (idx % kBitsPerInt));
        }
        totalAssigned_.fetch_add(1u, std::memory_order_release);
    }

    void unset(size_t idx) {
        if constexpr (kIsPowerOfTwo) {
            data_[idx / kBitsPerInt] &= ~(1u << (idx & (kBitsPerInt - 1u)));
        } else {
            data_[idx / kBitsPerInt] &= ~(1u << (idx % kBitsPerInt));
        }
        totalAssigned_.fetch_sub(1u, std::memory_order_release);
    }

    // Only used internally no need to do a bound check
    bool is_in_use(size_t idx) const {
        if constexpr (kIsPowerOfTwo) {
            return (data_[idx / kBitsPerInt] & (1u << (idx & (kBitsPerInt - 1u)))) != 0u;
        }
        return (data_[idx / kBitsPerInt] & (1u << (idx % kBitsPerInt))) != 0u;
    }

    std::atomic_uint totalAssigned_ = 0u;
};

} // namespace mp
