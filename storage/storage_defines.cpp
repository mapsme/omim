#include "storage/storage_defines.hpp"

namespace storage
{
string DebugPrint(TStatus status)
{
  switch (status)
  {
    case TStatus::EOnDisk:
      return "OnDisk";
    case TStatus::ENotDownloaded:
      return "NotDownloaded";
    case TStatus::EDownloadFailed:
      return "DownloadFailed";
    case TStatus::EDownloading:
      return "Downloading";
    case TStatus::EInQueue:
      return "InQueue";
    case TStatus::EUnknown:
      return "Unknown";
    case TStatus::EOnDiskOutOfDate:
      return "OnDiskOutOfDate";
    case TStatus::EOutOfMemFailed:
      return "OutOfMemFailed";
    case TStatus::EMixed:
      return "EMixed";
  case TStatus::EUndefined:
    return "EUndefined";
  }
}
}  // namespace storage
