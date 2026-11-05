#include "gui/trace_reader.hpp"

#include <cstring>
#include <fcntl.h>
#include <unistd.h>

using namespace Gui;
using namespace PhysicalLayer;

TraceReader::TraceReader(std::string path) : path(std::move(path)) {}

TraceReader::~TraceReader() {
  if (fd >= 0)
    ::close(fd);
}

const SignalData &TraceReader::signal(uint32_t pid, TraceStage stage) {
  return signals[{pid, stage}];
}

bool TraceReader::poll() {
  // O arquivo só existe depois que o canal é iniciado
  if (fd < 0) {
    fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0)
      return false;
  }

  std::byte chunk[1 << 16];
  ssize_t n;
  while ((n = ::read(fd, chunk, sizeof(chunk))) > 0)
    buffer.insert(buffer.end(), chunk, chunk + n);

  // Processa apenas registros completos (o canal pode estar escrevendo)
  bool updated = false;
  size_t offset = 0;

  while (buffer.size() - offset >= sizeof(TraceHeader)) {
    TraceHeader header;
    std::memcpy(&header, buffer.data() + offset, sizeof(header));

    if (buffer.size() - offset - sizeof(header) < header.size)
      break;

    process(header, buffer.data() + offset + sizeof(header));
    offset += sizeof(header) + header.size;
    updated = true;
  }

  buffer.erase(buffer.begin(), buffer.begin() + offset);
  return updated;
}

void TraceReader::process(const TraceHeader &header, const std::byte *payload) {
  SignalData &data = signals[{header.source_pid, header.stage}];

  // Pausa longa: nova transmissão
  if (data.last_time_ns &&
      header.time_ns - data.last_time_ns > BURST_GAP_NS) {
    data.samples.clear();
    data.pending.clear();
  }
  data.last_time_ns = header.time_ns;

  // O limite de um bloco pode cair no meio de uma amostra (8 bytes)
  data.pending.insert(data.pending.end(), payload, payload + header.size);

  const size_t count = data.pending.size() / sizeof(double);
  const size_t old_size = data.samples.size();

  data.samples.resize(old_size + count);
  std::memcpy(data.samples.data() + old_size, data.pending.data(),
              count * sizeof(double));
  data.pending.erase(data.pending.begin(),
                     data.pending.begin() + count * sizeof(double));
}
