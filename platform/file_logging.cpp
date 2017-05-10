#include "platform/file_logging.hpp"

#include <mutex>

#include "coding/file_writer.hpp"

#include "platform/platform.hpp"

#include <sstream>

namespace
{
  tm * GetLocalTime()
  {
    time_t rawTime;
    time(&rawTime);
    tm * localTime = localtime(&rawTime);
    assert(localTime);
    return localTime;
  }
}

void LogMessageFile(my::LogLevel level, my::SrcPoint const & srcPoint, std::string const & msg)
{
  static std::mutex mtx;
  static unique_ptr<FileWriter> file;

  std::string recordType;
  switch (level)
  {
  case LINFO: recordType.assign("INFO "); break;
  case LDEBUG: recordType.assign("DEBUG "); break;
  case LWARNING: recordType.assign("WARN "); break;
  case LERROR: recordType.assign("ERROR "); break;
  case LCRITICAL: recordType.assign("FATAL "); break;
  }

  std::lock_guard<std::mutex> lock(mtx);

  if (file == nullptr)
  {
    if (GetPlatform().WritableDir().empty())
      return;
    tm * curTimeTM = GetLocalTime();
    std::stringstream fileName;
    fileName << "logging_" << curTimeTM->tm_year + 1900 << "_" << curTimeTM->tm_mon + 1 << "_" << curTimeTM->tm_mday << "_"
      << curTimeTM->tm_hour << "_" << curTimeTM->tm_min << "_" << curTimeTM->tm_sec << ".log";
    file.reset(new FileWriter(GetPlatform().WritablePathForFile(fileName.str())));
  }

  std::string srcString = recordType + DebugPrint(srcPoint) + " " + msg + "\n";

  file->Write(srcString.c_str(), srcString.size());
  file->Flush();
}

void LogMemoryInfo()
{
  static unsigned long counter = 0;
  const unsigned short writeLogEveryNthCall = 3;
  if (counter++ % writeLogEveryNthCall == 0)
  {
    tm * curTimeTM = GetLocalTime();
    std::stringstream timeDate;
    timeDate << " " << curTimeTM->tm_year + 1900 << "." << curTimeTM->tm_mon + 1 << "." << curTimeTM->tm_mday << " "
      << curTimeTM->tm_hour << ":" << curTimeTM->tm_min << ":" << curTimeTM->tm_sec << " ";
    LOG(LINFO, (timeDate.str(), GetPlatform().GetMemoryInfo()));
  }
}
