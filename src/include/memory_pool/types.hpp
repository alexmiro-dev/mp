
#pragma once

#include <expected>
#include <string>

namespace mp::error {

enum class Code {
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
    Code code{Code::NoError};
    std::string detail;
};

inline auto unexp(Code code, std::string_view msg = "") {
    Result result{.code = code, .detail = msg.data()};
    return std::unexpected(std::move(result));
}

} // namespace mp::error
