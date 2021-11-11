
#include <array>
#include <sys/time.h>

namespace TCP {

namespace {
    constexpr size_t payload_size = 1472;
}

constexpr struct timeval tout = { .tv_sec = 3, .tv_usec = 0 };
constexpr struct timeval no_tout = { .tv_sec = 0, .tv_usec = 0 };

enum PacketType { DATA, ACK, SYN, ACKSYN, FIN, ACKFIN };

struct Payload {
    std::array<char, payload_size - sizeof(size_t) - sizeof(PacketType)> data;
    size_t size;
    size_t idx;
    PacketType type;
};

}
