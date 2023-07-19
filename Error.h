#pragma once
/*
 *  Error handle headers
 *  throw exception only when:
 *      1. Error case is happened rarely and it can't be handle in current context.
 *      2. Severe error is happen, such as the file descriptor is exhausted.
 *
 *  In other case, you'd better deal with error in current context or use std::option
 *  instead of throw exception.
 *
 * */

#include <cstdint>
extern "C" {
#include <netdb.h>
#include <string.h>
}
#include <stdexcept>
#include <system_error>

namespace utils {

enum class ErrorCode : int32_t {
    Success = 0,
    InvalidArgument,
    BadResult,
    OpNotAllowed,
    UnknownError,
};

// NormalException, which can be handled.
class NormalException : public std::runtime_error {
public:
    NormalException(const std::string& errMsg, ErrorCode errCode)
        : std::runtime_error(errMsg), mErrCode(errCode) {}

    [[nodiscard]]
    ErrorCode getErr() const noexcept { return mErrCode; }

private:
    ErrorCode mErrCode;
};

// NetworkException, wrapper of socket error.
class NetworkException : public std::runtime_error {
public:
    NetworkException(std::string errMsg, int errCode)
        : std::runtime_error(errMsg.append(gai_strerror(errCode))), mErrCode(errCode) {}

    [[nodiscard]]
    int getNetErr() const noexcept { return mErrCode; }
private:
    int mErrCode;
};

// SystemException, wrapper of system call error.
class SystemException : public std::system_error {
public:
    SystemException(const std::string& errMsg)
        : std::system_error(errno, std::system_category(), errMsg), mErrCode(errno) {}

    [[nodiscard]]
    int getSysErr() const noexcept { return mErrCode; }
private:
    int mErrCode;
};

// If other exception happen, re-throw it and make process abort.

}
