
#pragma once

#include "memory_pool/Allocator.hpp"
#include <expected>
#include <source_location>
#include <string>

namespace mp::error {

enum class Ecode {
    NoError = 0u,
    InternalLogicError,
    NotInitialized,
    CannotInitializeAgain,
    UnableToAllocateMemory,
    NoFreeSpace,
    ConstructorHasThrownException,
    DestructorHasThrownException,
    BucketIndexOutOfBounds,
};

struct Result {
    Ecode code = Ecode::NoError;
    std::string description = "";
    std::source_location source = std::source_location::current();

    static auto unexp(Result&& result) {
        return std::unexpected(std::move(result));
    }
};

inline auto unexp(Ecode code, std::string_view msg = "", std::source_location srcLoc = std::source_location::current()) {
    Result result{.code = code, .description = msg.data(), .source = srcLoc};
    return std::unexpected(std::move(result));
}

} // namespace mp::error
