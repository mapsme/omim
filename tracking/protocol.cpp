#include "tracking/protocol.hpp"

#include "coding/endianness.hpp"
#include "coding/writer.hpp"

#include "base/assert.hpp"

#include <cstdint>
#include <sstream>
#include <utility>

namespace
{
template <typename Container>
std::vector<uint8_t> CreateDataPacketImpl(Container const & points,
                                     tracking::Protocol::PacketType const type)
{
  std::vector<uint8_t> buffer;
  MemWriter<decltype(buffer)> writer(buffer);

  uint32_t version = tracking::Protocol::Encoder::kLatestVersion;
  switch (type)
  {
  case tracking::Protocol::PacketType::DataV0: version = 0; break;
  case tracking::Protocol::PacketType::DataV1: version = 1; break;
  case tracking::Protocol::PacketType::AuthV0: ASSERT(false, ("Not a DATA packet.")); break;
  }

  tracking::Protocol::Encoder::SerializeDataPoints(version, writer, points);

  auto packet = tracking::Protocol::CreateHeader(type, static_cast<uint32_t>(buffer.size()));
  packet.insert(packet.end(), begin(buffer), end(buffer));

  return packet;
}
}  // namespace

namespace tracking
{
uint8_t const Protocol::kOk[4] = {'O', 'K', '\n', '\n'};
uint8_t const Protocol::kFail[4] = {'F', 'A', 'I', 'L'};

static_assert(sizeof(Protocol::kFail) >= sizeof(Protocol::kOk), "");

//  static
std::vector<uint8_t> Protocol::CreateHeader(PacketType type, uint32_t payloadSize)
{
  std::vector<uint8_t> header;
  InitHeader(header, type, payloadSize);
  return header;
}

//  static
std::vector<uint8_t> Protocol::CreateAuthPacket(std::string const & clientId)
{
  std::vector<uint8_t> packet;

  InitHeader(packet, PacketType::CurrentAuth, static_cast<uint32_t>(clientId.size()));
  packet.insert(packet.end(), begin(clientId), end(clientId));

  return packet;
}

//  static
std::vector<uint8_t> Protocol::CreateDataPacket(DataElementsCirc const & points, PacketType type)
{
  return CreateDataPacketImpl(points, type);
}

//  static
std::vector<uint8_t> Protocol::CreateDataPacket(DataElementsVec const & points, PacketType type)
{
  return CreateDataPacketImpl(points, type);
}

//  static
std::pair<Protocol::PacketType, size_t> Protocol::DecodeHeader(std::vector<uint8_t> const & data)
{
  ASSERT_GREATER_OR_EQUAL(data.size(), sizeof(uint32_t /* header */), ());

  uint32_t size = (*reinterpret_cast<uint32_t const *>(data.data())) & 0xFFFFFF00;
  if (!IsBigEndian())
    size = ReverseByteOrder(size);

  return std::make_pair(PacketType(static_cast<uint8_t>(data[0])), size);
}

//  static
std::string Protocol::DecodeAuthPacket(Protocol::PacketType type, std::vector<uint8_t> const & data)
{
  switch (type)
  {
  case Protocol::PacketType::AuthV0: return std::string(begin(data), end(data));
  case Protocol::PacketType::DataV0:
  case Protocol::PacketType::DataV1: ASSERT(false, ("Not an AUTH packet.")); break;
  }
  return std::string();
}

//  static
Protocol::DataElementsVec Protocol::DecodeDataPacket(PacketType type, std::vector<uint8_t> const & data)
{
  DataElementsVec points;
  MemReader memReader(data.data(), data.size());
  ReaderSource<MemReader> src(memReader);
  switch (type)
  {
  case Protocol::PacketType::DataV0:
    Encoder::DeserializeDataPoints(0 /* version */, src, points);
    break;
  case Protocol::PacketType::DataV1:
    Encoder::DeserializeDataPoints(1 /* version */, src, points);
    break;
  case Protocol::PacketType::AuthV0: ASSERT(false, ("Not a DATA packet.")); break;
  }
  return points;
}

//  static
void Protocol::InitHeader(std::vector<uint8_t> & packet, PacketType type, uint32_t payloadSize)
{
  packet.resize(sizeof(uint32_t));
  uint32_t & size = *reinterpret_cast<uint32_t *>(packet.data());
  size = payloadSize;

  ASSERT_LESS(size, 0x00FFFFFF, ());

  if (!IsBigEndian())
    size = ReverseByteOrder(size);

  packet[0] = static_cast<uint8_t>(type);
}

std::string DebugPrint(Protocol::PacketType type)
{
  switch (type)
  {
  case Protocol::PacketType::AuthV0: return "AuthV0";
  case Protocol::PacketType::DataV0: return "DataV0";
  case Protocol::PacketType::DataV1: return "DataV1";
  }
  std::stringstream ss;
  ss << "Unknown(" << static_cast<uint32_t>(type) << ")";
  return ss.str();
}
}  // namespace tracking
