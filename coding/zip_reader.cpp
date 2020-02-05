#include "coding/zip_reader.hpp"

#include "coding/constants.hpp"

#include "base/logging.hpp"
#include "base/scope_guard.hpp"

#include "3party/minizip/unzip.h"

using namespace std;

namespace
{
class UnzipFileDelegate : public ZipFileReader::Delegate
{
public:
  explicit UnzipFileDelegate(string const & path)
    : m_file(make_unique<FileWriter>(path)), m_path(path), m_completed(false)
  {
  }

  ~UnzipFileDelegate() override
  {
    if (!m_completed)
    {
      m_file.reset();
      FileWriter::DeleteFileX(m_path);
    }
  }

  // ZipFileReader::Delegate overrides:
  void OnBlockUnzipped(size_t size, char const * data) override { m_file->Write(data, size); }

  void OnCompleted() override { m_completed = true; }

private:
  unique_ptr<FileWriter> m_file;
  string const m_path;
  bool m_completed;
};
}  // namespace

ZipFileReader::ZipFileReader(string const & container, string const & file, uint32_t logPageSize,
                             uint32_t logPageCount)
  : FileReader(container, logPageSize, logPageCount), m_uncompressedFileSize(0)
{
  unzFile zip = munzOpen64(container.c_str());
  if (!zip)
    MYTHROW(OpenZipException, ("Can't get zip file handle", container));

  SCOPE_GUARD(zipGuard, bind(&munzClose, zip));

  if (UNZ_OK != munzLocateFile(zip, file.c_str(), 1))
    MYTHROW(LocateZipException, ("Can't locate file inside zip", file));

  if (UNZ_OK != munzOpenCurrentFile(zip))
    MYTHROW(LocateZipException, ("Can't open file inside zip", file));

  uint64_t const offset = munzGetCurrentFileZStreamPos64(zip);
  (void) munzCloseCurrentFile(zip);

  if (offset == 0 || offset > Size())
    MYTHROW(LocateZipException, ("Invalid offset inside zip", file));

  unz_file_info64 fileInfo;
  if (UNZ_OK != munzGetCurrentFileInfo64(zip, &fileInfo, NULL, 0, NULL, 0, NULL, 0))
    MYTHROW(LocateZipException, ("Can't get compressed file size inside zip", file));

  SetOffsetAndSize(offset, fileInfo.compressed_size);
  m_uncompressedFileSize = fileInfo.uncompressed_size;
}

void ZipFileReader::FilesList(string const & zipContainer, FileList & filesList)
{
  unzFile const zip = munzOpen64(zipContainer.c_str());
  if (!zip)
    MYTHROW(OpenZipException, ("Can't get zip file handle", zipContainer));

  SCOPE_GUARD(zipGuard, bind(&munzClose, zip));

  if (UNZ_OK != munzGoToFirstFile(zip))
    MYTHROW(LocateZipException, ("Can't find first file inside zip", zipContainer));

  do
  {
    char fileName[256];
    unz_file_info64 fileInfo;
    if (UNZ_OK != munzGetCurrentFileInfo64(zip, &fileInfo, fileName, ARRAY_SIZE(fileName), NULL, 0, NULL, 0))
      MYTHROW(LocateZipException, ("Can't get file name inside zip", zipContainer));

    filesList.push_back(make_pair(fileName, fileInfo.uncompressed_size));

  } while (UNZ_OK == munzGoToNextFile(zip));
}

bool ZipFileReader::IsZip(string const & zipContainer)
{
  unzFile zip = munzOpen64(zipContainer.c_str());
  if (!zip)
    return false;
  munzClose(zip);
  return true;
}

// static
void ZipFileReader::UnzipFile(string const & zipContainer, string const & fileInZip,
                              Delegate & delegate)
{
  unzFile zip = munzOpen64(zipContainer.c_str());
  if (!zip)
    MYTHROW(OpenZipException, ("Can't get zip file handle", zipContainer));
  SCOPE_GUARD(zipGuard, bind(&munzClose, zip));

  if (UNZ_OK != munzLocateFile(zip, fileInZip.c_str(), 1))
    MYTHROW(LocateZipException, ("Can't locate file inside zip", fileInZip));

  if (UNZ_OK != munzOpenCurrentFile(zip))
    MYTHROW(LocateZipException, ("Can't open file inside zip", fileInZip));
  SCOPE_GUARD(currentFileGuard, bind(&munzCloseCurrentFile, zip));

  unz_file_info64 fileInfo;
  if (UNZ_OK != munzGetCurrentFileInfo64(zip, &fileInfo, NULL, 0, NULL, 0, NULL, 0))
    MYTHROW(LocateZipException, ("Can't get uncompressed file size inside zip", fileInZip));

  char buf[ZIP_FILE_BUFFER_SIZE];
  int readBytes = 0;

  delegate.OnStarted();
  do
  {
    readBytes = munzReadCurrentFile(zip, buf, ZIP_FILE_BUFFER_SIZE);
    if (readBytes < 0)
    {
      MYTHROW(InvalidZipException,
              ("Error", readBytes, "while unzipping", fileInZip, "from", zipContainer));
    }

    delegate.OnBlockUnzipped(static_cast<size_t>(readBytes), buf);
  } while (readBytes != 0);
  delegate.OnCompleted();
}

// static
void ZipFileReader::UnzipFile(string const & zipContainer, string const & fileInZip,
                              string const & outPath)
{
  UnzipFileDelegate delegate(outPath);
  UnzipFile(zipContainer, fileInZip, delegate);
}
