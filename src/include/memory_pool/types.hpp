
#pragma once

#include <cstdint>
#include <expected>
#include <source_location>
#include <string>

namespace mp::error {

enum class code_e : std::uint32_t {
    ok = 0u,
    bad_logic,
    not_initialized,
    already_initialized,
    system_memory_is_full,
    allocator_is_full,
    exception_caught_in_ctor,
    exception_caught_in_dctor,
    out_of_bounds
};

struct result_t {
    code_e code = code_e::ok;
    std::string description = "";
    std::source_location source = std::source_location::current();

    static auto unexp(result_t &&result) { return std::unexpected(std::move(result)); }
};

} // namespace mp::error
