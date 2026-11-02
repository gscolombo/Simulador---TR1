#ifndef TYPES_H
#define TYPES_H

#define BUF_SIZE 4096

#include <cstddef>
#include <cstdint>
#include <sys/socket.h>
#include <sys/un.h>
#include <utility>
#include <vector>

namespace PhysicalLayer {

typedef std::vector<std::byte> bytes;
typedef uint8_t bit; // Must be asserted to 0 or 1
typedef std::vector<bit> symbol;
typedef std::vector<double> signal_sample;

typedef std::pair<double, double> IQ_coord;
typedef std::vector<std::pair<char, IQ_coord>> IQ_map;

constexpr struct sockaddr_un channel = {AF_UNIX, "/tmp/channel.sock"};
} // namespace PhysicalLayer

#endif