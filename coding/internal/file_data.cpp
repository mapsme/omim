#include "coding/internal/file_data.hpp"

#include "coding/constants.hpp"
#include "coding/reader.hpp" // For Reader exceptions.
#include "coding/writer.hpp" // For Writer exceptions.

#include "base/exception.hpp"
#include "base/logging.hpp"
#include "base/string_utils.hpp"

#include "std/target_os.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <exception>
#include <fstream>
#include <thread>
#include <vector>

#ifdef OMIM_OS_WINDOWS
#include <io.h>
#endif

using namespace std;

namespace base
{
FileData::FileData(string const & fileName, Op op)
  : m_FileName(fileName), m_Op(op)
{
  char const * const modes [] = {"rb", "wb", "r+b", "ab"};

  m_File = fopen(fileName.c_str(), modes[op]);
  if (m_File)
    return;

  if (op == OP_WRITE_EXISTING)
  {
    // Special case, since "r+b" fails if file doesn't exist.
    m_File = fopen(fileName.c_str(), "wb");
    if (m_File)
      return;
  }

  // if we're here - something bad is happened
  if (m_Op != OP_READ)
    MYTHROW(Writer::OpenException, (GetErrorProlog()));
  else
    MYTHROW(Reader::OpenException, (GetErrorProlog()));
}

FileData::~FileData()
{
  if (m_File)
  {
    if (fclose(m_File))
      LOG(LWARNING, ("Error closing file", GetErrorProlog()));
  }
}

string FileData::GetErrorProlog() const
{
  char const * s;
  switch (m_Op)
  {
  case OP_READ: s = "Read"; break;
  case OP_WRITE_TRUNCATE: s = "Write truncate"; break;
  case OP_WRITE_EXISTING: s = "Write existing"; break;
  case OP_APPEND: s = "Append"; break;
  }

  return m_FileName + "; " + s + "; " + strerror(errno);
}

static int64_t const INVALID_POS = -1;

uint64_t FileData::Size() const
{
  int64_t const pos = ftell64(m_File);
  if (pos == INVALID_POS)
    MYTHROW(Reader::SizeException, (GetErrorProlog(), pos));

  if (fseek64(m_File, 0, SEEK_END))
    MYTHROW(Reader::SizeException, (GetErrorProlog()));

  int64_t const size = ftell64(m_File);
  if (size == INVALID_POS)
    MYTHROW(Reader::SizeException, (GetErrorProlog(), size));

  if (fseek64(m_File, static_cast<off_t>(pos), SEEK_SET))
    MYTHROW(Reader::SizeException, (GetErrorProlog(), pos));

  ASSERT_GREATER_OR_EQUAL(size, 0, ());
  return static_cast<uint64_t>(size);
}

void FileData::Read(uint64_t pos, void * p, size_t size)
{
  if (fseek64(m_File, static_cast<off_t>(pos), SEEK_SET))
    MYTHROW(Reader::ReadException, (GetErrorProlog(), pos));

  size_t const bytesRead = fread(p, 1, size, m_File);
  if (bytesRead != size || ferror(m_File))
    MYTHROW(Reader::ReadException, (GetErrorProlog(), bytesRead, pos, size));
}

uint64_t FileData::Pos() const
{
  int64_t const pos = ftell64(m_File);
  if (pos == INVALID_POS)
    MYTHROW(Writer::PosException, (GetErrorProlog(), pos));

  ASSERT_GREATER_OR_EQUAL(pos, 0, ());
  return static_cast<uint64_t>(pos);
}

void FileData::Seek(uint64_t pos)
{
  ASSERT_NOT_EQUAL(m_Op, OP_APPEND, (m_FileName, m_Op, pos));
  if (fseek64(m_File, static_cast<off_t>(pos), SEEK_SET))
    MYTHROW(Writer::SeekException, (GetErrorProlog(), pos));
}

void FileData::Write(void const * p, size_t size)
{
  size_t const bytesWritten = fwrite(p, 1, size, m_File);
  if (bytesWritten != size || ferror(m_File))
    MYTHROW(Writer::WriteException, (GetErrorProlog(), bytesWritten, size));
}

void FileData::Flush()
{
  if (fflush(m_File))
    MYTHROW(Writer::WriteException, (GetErrorProlog()));
}

void FileData::Truncate(uint64_t sz)
{
#ifdef OMIM_OS_WINDOWS
  int const res = _chsize(fileno(m_File), sz);
#else
  int const res = ftruncate(fileno(m_File), static_cast<off_t>(sz));
#endif

  if (res)
    MYTHROW(Writer::WriteException, (GetErrorProlog(), sz));
}

bool GetFileSize(string const & fName, uint64_t & sz)
{
  try
  {
    typedef base::FileData fdata_t;
    fdata_t f(fName, fdata_t::OP_READ);
    sz = f.Size();
    return true;
  }
  catch (RootException const &)
  {
    // supress all exceptions here
    return false;
  }
}

namespace
{
bool CheckFileOperationResult(int res, string const & fName)
{
  if (!res)
    return true;

  LOG(LWARNING, ("File operation error for file:", fName, "-", strerror(errno)));

  // additional check if file really was removed correctly
  uint64_t dummy;
  if (GetFileSize(fName, dummy))
  {
    LOG(LERROR, ("File exists but can't be deleted. Sharing violation?", fName));
  }

  return false;
}
}  // namespace

bool DeleteFileX(string const & fName)
{
  int res = remove(fName.c_str());
  return CheckFileOperationResult(res, fName);
}

bool RenameFileX(string const & fOld, string const & fNew)
{
  int res = rename(fOld.c_str(), fNew.c_str());
  return CheckFileOperationResult(res, fOld);
}

bool WriteToTempAndRenameToFile(string const & dest, function<bool(string const &)> const & write,
                                string const & tmp)
{
  string const tmpFileName =
      tmp.empty() ? dest + ".tmp" + strings::to_string(this_thread::get_id()) : tmp;
  if (!write(tmpFileName))
  {
    LOG(LERROR, ("Can't write to", tmpFileName));
    DeleteFileX(tmpFileName);
    return false;
  }
  if (!RenameFileX(tmpFileName, dest))
  {
    LOG(LERROR, ("Can't rename file", tmpFileName, "to", dest));
    DeleteFileX(tmpFileName);
    return false;
  }
  return true;
}

void AppendFileToFile(string const & fromFilename, string const & toFilename)
{
  ifstream from;
  from.exceptions(fstream::failbit | fstream::badbit);
  from.open(fromFilename, ios::binary);

  ofstream to;
  to.exceptions(fstream::badbit);
  to.open(toFilename, ios::binary | ios::app);

  auto * buffer = from.rdbuf();
  if (from.peek() != ifstream::traits_type::eof())
    to << buffer;
}

bool CopyFileX(string const & fOld, string const & fNew)
{
  try
  {
    ifstream ifs(fOld.c_str());
    ofstream ofs(fNew.c_str());

    if (ifs.is_open() && ofs.is_open())
    {
      if (ifs.peek() == ifstream::traits_type::eof())
        return true;

      ofs << ifs.rdbuf();
      ofs.flush();

      if (ofs.fail())
      {
        LOG(LWARNING, ("Bad or Fail bit is set while writing file:", fNew));
        return false;
      }

      return true;
    }
    else
      LOG(LERROR, ("Can't open files:", fOld, fNew));
  }
  catch (exception const & ex)
  {
    LOG(LERROR, ("Copy file error:", ex.what()));
  }

  return false;
}

bool IsEqualFiles(string const & firstFile, string const & secondFile)
{
  base::FileData first(firstFile, base::FileData::OP_READ);
  base::FileData second(secondFile, base::FileData::OP_READ);
  if (first.Size() != second.Size())
    return false;

  size_t const bufSize = READ_FILE_BUFFER_SIZE;
  vector<char> buf1, buf2;
  buf1.resize(bufSize);
  buf2.resize(bufSize);
  size_t const fileSize = static_cast<size_t>(first.Size());
  size_t currSize = 0;

  while (currSize < fileSize)
  {
    size_t const toRead = min(bufSize, fileSize - currSize);

    first.Read(currSize, &buf1[0], toRead);
    second.Read(currSize, &buf2[0], toRead);

    if (buf1 != buf2)
      return false;

    currSize += toRead;
  }

  return true;
}
}  // namespace base
