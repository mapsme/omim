#include "mwm_version.hpp"

#include "coding/file_container.hpp"
#include "coding/reader_wrapper.hpp"
#include "coding/varint.hpp"
#include "coding/writer.hpp"

#include "base/string_utils.hpp"
#include "base/timer.hpp"

#include "defines.hpp"

#include "std/array.hpp"

#include <boost/date_time/gregorian/gregorian_types.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

namespace version
{
namespace
{
boost::posix_time::ptime const kEpoch({1970, 1, 1});

int64_t PTimeToSecondsSinceEpoch(boost::posix_time::ptime ptime)
{
  return (boost::posix_time::ptime(ptime) - kEpoch).total_seconds();
}

char const MWM_PROLOG[] = "MWM";

template <class TSource>
void ReadVersionT(TSource & src, MwmVersion & version)
{
  size_t const prologSize = ARRAY_SIZE(MWM_PROLOG);
  char prolog[prologSize];
  src.Read(prolog, prologSize);

  if (strcmp(prolog, MWM_PROLOG) != 0)
  {
    version.SetFormat(Format::v2);
    version.SetVersion(my::GenerateYYMMDDHHMMSS(2011 - 1900, 10, 1, 0, 0, 0));
    return;
  }

  // Read format value "as-is". It's correctness will be checked later
  // with the correspondent return value.
  version.SetFormat(static_cast<Format>(ReadVarUint<uint32_t>(src)));
  if (version.GetFormat() < Format::v9)
    version.SetVersion(ReadVarUint<uint32_t>(src));
  else
    version.SetVersion(ReadVarUint<uint64_t>(src));
}
}  // namespace

MwmVersion::MwmVersion() : m_format(Format::unknownFormat), m_version(0) {}

int64_t MwmVersion::GetTimestamp() const
{
  auto constexpr partsCount = 6;
  auto version = m_version;
  // From left to right: YY MM DD HH MM SS.
  array<int, partsCount> parts{};  // Initialize with zeros.
  for (auto i = (m_format< Format::v9 ? 3 : 0); i < partsCount; ++i)
  {
    parts[partsCount - i - 1] = version % 100;
    version /= 100;
  }
  return PTimeToSecondsSinceEpoch({boost::gregorian::date(2000 + parts[0], parts[1], parts[2]),
                                   boost::posix_time::time_duration(parts[3], parts[4], parts[5])});
}

string DebugPrint(Format f)
{
  return "v" + strings::to_string(static_cast<uint32_t>(f) + 1);
}

void WriteVersion(Writer & w, uint64_t versionDate)
{
  w.Write(MWM_PROLOG, ARRAY_SIZE(MWM_PROLOG));

  // write inner data version
  WriteVarUint(w, static_cast<uint32_t>(Format::lastFormat));
  WriteVarUint(w, versionDate);
}

void ReadVersion(ReaderSrc & src, MwmVersion & version) { ReadVersionT(src, version); }

bool ReadVersion(FilesContainerR const & container, MwmVersion & version)
{
  if (!container.IsExist(VERSION_FILE_TAG))
    return false;

  ModelReaderPtr versionReader = container.GetReader(VERSION_FILE_TAG);
  ReaderSource<ModelReaderPtr> src(versionReader);
  ReadVersionT(src, version);
  return true;
}

uint64_t ReadVersionValue(ModelReaderPtr const & reader)
{
  MwmVersion version;
  if (!ReadVersion(FilesContainerR(reader), version))
    return 0;

  return version.GetVersion();
}

bool IsSingleMwm(uint64_t version)
{
  #pragma message("Check this version and move if necessary before small mwm release.")
  uint64_t constexpr kMinSingleMwmVersion = 160107;
  return version >= kMinSingleMwmVersion || version == 0 /* Version of mwm in the root directory. */;
}
}  // namespace version
