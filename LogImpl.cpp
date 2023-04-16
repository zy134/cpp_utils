#include "Log.h"
#include "format.h"
#include "utils.h"
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <array>

using namespace utils;
using namespace utils::detail;
namespace THISSPACE = utils::detail;

class THISSPACE::LogBuffer {
    DISABLE_COPY(LogBuffer);
public:

private:
    std::unique_ptr<std::array<uint8_t, 4096>>  mpRawBuffer;
    std::array<uint8_t, 4096>::size_type        mUsedSize;
};

class THISSPACE::LogServer {
    DISABLE_COPY(LogServer);
    DISABLE_MOVE(LogServer);
public:
    std::shared_ptr<LogServer> getInstance();
    LogServer() {}
    ~LogServer() {}

    void write_info(std::string_view fmt);
    void write_err(std::string_view fmt);
private:
    void flush_normal_buffer(LogBuffer& buffer);
    void flush_hi_buffer(LogBuffer& buffer);
};
