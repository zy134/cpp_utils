#pragma once

#include "utils.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>
#include <system_error>
#include <span>
extern "C" {
#include <fcntl.h>
#include <unistd.h>
}
namespace utils {

class FileDesc {
    DISABLE_COPY(FileDesc);
public:
    explicit FileDesc(std::string_view path, int flags) {
        mFd = open(path.data(), flags);
        if (mFd < 0) {
            throw std::system_error { errno, std::system_category(), "Can't open file"};
        }
    }

    ~FileDesc() {
        if (mFd >= 0)
            close(mFd);
    }

    [[nodiscard]]
    int getRawFd() const { return mFd; }

    int write(std::string_view str);

    template <typename T>
    int write(std::span<T> buffer);

    template <typename T, size_t Size>
    int write(std::span<T, Size> buffer);

    int read(uint8_t* buf, size_t size);

    template <typename T>
    int read(std::span<T> buffer, size_t size);

    template <typename T, size_t Size>
    int read(std::span<T, Size> buffer);

    void setNoBlock();
    bool isNoBlock();

    void setNoDelay();
    void isNoDelay();

private:
    int mFd;
};
} // namespace utils
