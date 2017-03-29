#include "localads/campaign_serialization.hpp"

#include "coding/varint.hpp"
#include "coding/byte_stream.hpp"

#include <vector>
#include <type_traits>
#include <cstdint>


namespace
{
enum class Version
{
  unknown = -1,
  v1 = 0,  // March 2017 (Store featrues ids as varints, use two bytes for
           // both days before expiration and icon id.)
  latest = v1
};

template<typename ByteStream, typename T>
void Write(ByteStream & s, T t)
{
  s.Write(&t, sizeof(T));
}

template<typename T, typename ByteStream>
T Read(ByteStream & s)
{
  T t;
  s.Read(static_cast<void*>(&t), sizeof(t));
  return t;
}

template<typename Integral,
         typename ByteStream,
         typename std::enable_if<std::is_integral<Integral>::value, void*>::type = nullptr>
std::vector<Integral> ReadData(ByteStream & s, size_t chunksNumber)
{
  std::vector<Integral> result;
  while (chunksNumber--)
    result.push_back(ReadVarUint<uint32_t>(s));
  return result;
}
}  // namespace

namespace localads
{
std::vector<uint8_t> Serialize(std::vector<Campaign> const & campaigns)
{
  std::vector<uint8_t> buff;
  PushBackByteSink<decltype(buff)> dst(buff);
  Write(dst, Version::latest);
  Write(dst, campaigns.size());
  for (auto const & c : campaigns)
     WriteVarUint(dst, c.m_featureId);
  for (auto const & c : campaigns)
    WriteVarUint(dst, c.m_iconId);

  for (auto const & c : campaigns)
    Write(dst, c.m_daysBeforeExpired);
  return buff;
}

std::vector<Campaign> Deserialize(std::vector<uint8_t> const & bytes)
{
  ArrayByteSource src(bytes.data());
  auto const version = Read<Version>(src);  // No version dispatching for now.
  auto const chunksNumber = Read<size_t>(src);

  auto const featureIds = ReadData<uint32_t>(src, chunksNumber);
  auto const icons = ReadData<uint16_t>(src, chunksNumber);
  auto const expirations = ReadData<uint8_t>(src, chunksNumber);

  CHECK_EQUAL(featureIds.size(), chunksNumber, ());
  CHECK_EQUAL(icons.size(), chunksNumber, ());
  CHECK_EQUAL(expirations.size(), chunksNumber, ());

  std::vector<Campaign> campaigns;
  for (size_t i = 0; i < chunksNumber; ++i)
    campaigns.emplace_back(featureIds[i], icons[i], expirations[i], 0);
  return campaigns;
}
}  // namespace localads
