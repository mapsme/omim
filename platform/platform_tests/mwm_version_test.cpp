#include "testing/testing.hpp"

#include "coding/reader_wrapper.hpp"
#include "coding/varint.hpp"
#include "coding/writer.hpp"

#include "platform/mwm_version.hpp"

#include "std/string.hpp"

namespace
{
string WriteMwmVersion(version::Format const format, uint64_t const version)
{
  string data;
  MemWriter<string> writer(data);
  WriterSink<MemWriter<string>> sink(writer);

  char const prolog[] = "MWM";
  sink.Write(prolog, ARRAY_SIZE(prolog));

  WriteVarUint(sink, static_cast<uint32_t>(format));
  WriteVarUint(sink, version);

  return data;
}

version::MwmVersion ReadMwmVersion(string const & data)
{
  MemReader reader(data.data(), data.size());
  ReaderSrc source(reader);

  version::MwmVersion version;
  version::ReadVersion(source, version);

  return version;
}
}  // namespace

UNIT_TEST(MwmVersion_OldFormat)
{
  auto const timestamp = 1455235200;  // 160212
  auto const dataVersion = 160212;
  auto const data = WriteMwmVersion(version::Format::v8, dataVersion);
  auto const mwmVersion = ReadMwmVersion(data);
  TEST_EQUAL(mwmVersion.GetTimestamp(), timestamp, ());
  TEST_EQUAL(mwmVersion.GetFormat(), version::Format::v8, ());
  TEST_EQUAL(mwmVersion.GetVersion(), dataVersion, ());
}

UNIT_TEST(MwmVersion_NewFormat)
{
  auto const timestamp = 1455870947;  // 160219083546
  auto const dataVersion = 160219083547;
  auto const data = WriteMwmVersion(version::Format::v9, dataVersion);
  auto const mwmVersion = ReadMwmVersion(data);
  TEST_EQUAL(mwmVersion.GetTimestamp(), timestamp, ());
  TEST_EQUAL(mwmVersion.GetFormat(), version::Format::v9, ());
  TEST_EQUAL(mwmVersion.GetVersion(), dataVersion, ());
}
