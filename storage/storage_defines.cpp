#include "storage/storage_defines.hpp"

#include <sstream>

namespace storage
{
std::string DebugPrint(Status status)
{
  switch (status)
  {
  case Status::EUndefined:
    return std::string("EUndefined");
  case Status::EOnDisk:
    return std::string("OnDisk");
  case Status::ENotDownloaded:
    return std::string("NotDownloaded");
  case Status::EDownloadFailed:
    return std::string("DownloadFailed");
  case Status::EDownloading:
    return std::string("Downloading");
  case Status::EInQueue:
    return std::string("InQueue");
  case Status::EUnknown:
    return std::string("Unknown");
  case Status::EOnDiskOutOfDate:
    return std::string("OnDiskOutOfDate");
  case Status::EOutOfMemFailed:
    return std::string("OutOfMemFailed");
  }
}

std::string DebugPrint(NodeStatus status)
{
  switch (status)
  {
  case NodeStatus::Undefined:
    return std::string("Undefined");
  case NodeStatus::Error:
    return std::string("Error");
  case NodeStatus::OnDisk:
    return std::string("OnDisk");
  case NodeStatus::NotDownloaded:
    return std::string("NotDownloaded");
  case NodeStatus::Downloading:
    return std::string("Downloading");
  case NodeStatus::InQueue:
    return std::string("InQueue");
  case NodeStatus::OnDiskOutOfDate:
    return std::string("OnDiskOutOfDate");
  case NodeStatus::Partly:
    return std::string("Partly");
  }
}

std::string DebugPrint(NodeErrorCode status)
{
  switch (status)
  {
  case NodeErrorCode::NoError:
    return std::string("NoError");
  case NodeErrorCode::UnknownError:
    return std::string("UnknownError");
  case NodeErrorCode::OutOfMemFailed:
    return std::string("OutOfMemFailed");
  case NodeErrorCode::NoInetConnection:
    return std::string("NoInetConnection");
  }
}

StatusAndError ParseStatus(Status innerStatus)
{
  switch (innerStatus)
  {
  case Status::EUndefined:
    return StatusAndError(NodeStatus::Undefined, NodeErrorCode::NoError);
  case Status::EOnDisk:
    return StatusAndError(NodeStatus::OnDisk, NodeErrorCode::NoError);
  case Status::ENotDownloaded:
    return StatusAndError(NodeStatus::NotDownloaded, NodeErrorCode::NoError);
  case Status::EDownloadFailed:
    return StatusAndError(NodeStatus::Error, NodeErrorCode::NoInetConnection);
  case Status::EDownloading:
    return StatusAndError(NodeStatus::Downloading, NodeErrorCode::NoError);
  case Status::EInQueue:
    return StatusAndError(NodeStatus::InQueue, NodeErrorCode::NoError);
  case Status::EUnknown:
    return StatusAndError(NodeStatus::Error, NodeErrorCode::UnknownError);
  case Status::EOnDiskOutOfDate:
    return StatusAndError(NodeStatus::OnDiskOutOfDate, NodeErrorCode::NoError);
  case Status::EOutOfMemFailed:
    return StatusAndError(NodeStatus::Error, NodeErrorCode::OutOfMemFailed);
  }
}

std::string DebugPrint(StatusAndError statusAndError)
{
  std::ostringstream out;
  out << "StatusAndError[" << DebugPrint(statusAndError.status)
      << ", " << DebugPrint(statusAndError.error) << "]";
  return out.str();
}
}  // namespace storage
