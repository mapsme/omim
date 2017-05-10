#pragma once

#include "coding/file_reader.hpp"
#include "coding/file_writer.hpp"

#include "base/exception.hpp"

#include <functional>
#include <utility>


class ZipFileReader : public FileReader
{
private:
  uint64_t m_uncompressedFileSize;

public:
  struct Delegate
  {
    virtual ~Delegate() = default;

    // When |size| is zero, end of file is reached.
    virtual void OnBlockUnzipped(size_t size, char const * data) = 0;

    virtual void OnStarted() {}
    virtual void OnCompleted() {}
  };

  typedef std::function<void(uint64_t, uint64_t)> ProgressFn;
  /// Contains file name inside zip and it's uncompressed size
  typedef std::vector<std::pair<std::string, uint32_t> > FileListT;

  DECLARE_EXCEPTION(OpenZipException, OpenException);
  DECLARE_EXCEPTION(LocateZipException, OpenException);
  DECLARE_EXCEPTION(InvalidZipException, OpenException);

  /// @param[in] logPageSize, logPageCount default values are equal with FileReader constructor.
  ZipFileReader(std::string const & container, std::string const & file,
                uint32_t logPageSize = 10, uint32_t logPageCount = 4);

  /// @note Size() returns compressed file size inside zip
  uint64_t UncompressedSize() const { return m_uncompressedFileSize; }

  /// @warning Can also throw Writer::OpenException and Writer::WriteException
  static void UnzipFile(std::string const & zipContainer, std::string const & fileInZip, Delegate & delegate);
  static void UnzipFile(std::string const & zipContainer, std::string const & fileInZip,
                        std::string const & outPath);

  static void FilesList(std::string const & zipContainer, FileListT & filesList);

  /// Quick version without exceptions
  static bool IsZip(std::string const & zipContainer);
};
