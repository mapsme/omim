#pragma once
#include "coding/internal/file64_api.hpp"

#include "base/base.hpp"

#include <functional>
#include <boost/noncopyable.hpp>
#include <string>
#include "std/target_os.hpp"

#ifdef OMIM_OS_TIZEN
namespace Tizen
{
  namespace Io
  {
    class File;
  }
}
#endif


namespace my {

class FileData: private boost::noncopyable
{
public:
  /// @note Do not change order (@see FileData::FileData).
  enum Op { OP_READ = 0, OP_WRITE_TRUNCATE, OP_WRITE_EXISTING, OP_APPEND };

  FileData(std::string const & fileName, Op op);
  ~FileData();

  uint64_t Size() const;
  uint64_t Pos() const;

  void Seek(uint64_t pos);

  void Read(uint64_t pos, void * p, size_t size);
  void Write(void const * p, size_t size);

  void Flush();
  void Truncate(uint64_t sz);

  std::string const & GetName() const { return m_FileName; }

private:

#ifdef OMIM_OS_TIZEN
  Tizen::Io::File * m_File;
#else
  FILE * m_File;
#endif
  std::string m_FileName;
  Op m_Op;

  std::string GetErrorProlog() const;
};

bool GetFileSize(std::string const & fName, uint64_t & sz);
bool DeleteFileX(std::string const & fName);
bool RenameFileX(std::string const & fOld, std::string const & fNew);

/// Write to temp file and rename it to dest. Delete temp on failure.
/// @param write function that writes to file with a given name, returns true on success.
bool WriteToTempAndRenameToFile(std::string const & dest, std::function<bool(std::string const &)> const & write,
                                std::string const & tmp = "");

/// @return false if copy fails. DO NOT THROWS exceptions
bool CopyFileX(std::string const & fOld, std::string const & fNew);
bool IsEqualFiles(std::string const & firstFile, std::string const & secondFile);
}
