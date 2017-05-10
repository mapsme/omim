#include "coding/file_reader.hpp"
#include "coding/reader_cache.hpp"
#include "coding/internal/file_data.hpp"

#ifndef LOG_FILE_READER_STATS
#define LOG_FILE_READER_STATS 0
#endif // LOG_FILE_READER_STATS

#if LOG_FILE_READER_STATS && !defined(LOG_FILE_READER_EVERY_N_READS_MASK)
#define LOG_FILE_READER_EVERY_N_READS_MASK 0xFFFFFFFF
#endif

namespace
{
  class FileDataWithCachedSize : public my::FileData
  {
      typedef my::FileData base_t;

  public:
    explicit FileDataWithCachedSize(std::string const & fileName)
      : base_t(fileName, FileData::OP_READ), m_Size(FileData::Size()) {}

    uint64_t Size() const { return m_Size; }

  private:
    uint64_t m_Size;
  };
}

class FileReader::FileReaderData
{
public:
  FileReaderData(std::string const & fileName, uint32_t logPageSize, uint32_t logPageCount)
    : m_FileData(fileName), m_ReaderCache(logPageSize, logPageCount)
  {
#if LOG_FILE_READER_STATS
    m_ReadCallCount = 0;
#endif
  }

  ~FileReaderData()
  {
#if LOG_FILE_READER_STATS
    LOG(LINFO, ("FileReader", GetName(), m_ReaderCache.GetStatsStr()));
#endif
  }

  uint64_t Size() const { return m_FileData.Size(); }

  void Read(uint64_t pos, void * p, size_t size)
  {
#if LOG_FILE_READER_STATS
    if (((++m_ReadCallCount) & LOG_FILE_READER_EVERY_N_READS_MASK) == 0)
    {
      LOG(LINFO, ("FileReader", GetName(), m_ReaderCache.GetStatsStr()));
    }
#endif

    return m_ReaderCache.Read(m_FileData, pos, p, size);
  }

private:
  FileDataWithCachedSize m_FileData;
  ReaderCache<FileDataWithCachedSize, LOG_FILE_READER_STATS> m_ReaderCache;

#if LOG_FILE_READER_STATS
  uint32_t m_ReadCallCount;
#endif
};

FileReader::FileReader(std::string const & fileName, bool withExceptions,
                       uint32_t logPageSize, uint32_t logPageCount)
  : BaseType(fileName)
  , m_fileData(new FileReaderData(fileName, logPageSize, logPageCount))
  , m_offset(0)
  , m_size(m_fileData->Size())
  , m_withExceptions(withExceptions)
{}

FileReader::FileReader(FileReader const & reader, uint64_t offset, uint64_t size)
  : BaseType(reader.GetName())
  , m_fileData(reader.m_fileData)
  , m_offset(offset)
  , m_size(size)
  , m_withExceptions(reader.m_withExceptions)
{}

uint64_t FileReader::Size() const
{
  return m_size;
}

void FileReader::Read(uint64_t pos, void * p, size_t size) const
{
  CheckPosAndSize(pos, size);
  m_fileData->Read(m_offset + pos, p, size);
}

FileReader FileReader::SubReader(uint64_t pos, uint64_t size) const
{
  CheckPosAndSize(pos, size);
  return FileReader(*this, m_offset + pos, size);
}

std::unique_ptr<Reader> FileReader::CreateSubReader(uint64_t pos, uint64_t size) const
{
  CheckPosAndSize(pos, size);
  // Can't use make_unique with private constructor.
  return std::unique_ptr<Reader>(new FileReader(*this, m_offset + pos, size));
}

bool FileReader::AssertPosAndSize(uint64_t pos, uint64_t size) const
{
  uint64_t const allSize1 = Size();
  bool const ret1 = (pos + size <= allSize1);
  if (!m_withExceptions)
    ASSERT(ret1, (pos, size, allSize1));

  uint64_t const allSize2 = m_fileData->Size();
  bool const ret2 = (m_offset + pos + size <= allSize2);
  if (!m_withExceptions)
    ASSERT(ret2, (m_offset, pos, size, allSize2));

  return (ret1 && ret2);
}

void FileReader::CheckPosAndSize(uint64_t pos, uint64_t size) const
{
  if (m_withExceptions && !AssertPosAndSize(pos, size))
    MYTHROW(Reader::SizeException, (pos, size));
  else
    ASSERT(AssertPosAndSize(pos, size), ());
}

void FileReader::SetOffsetAndSize(uint64_t offset, uint64_t size)
{
  CheckPosAndSize(offset, size);
  m_offset = offset;
  m_size = size;
}
