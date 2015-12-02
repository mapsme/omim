#include "map/gps_track_file.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"

namespace
{

inline fstream CreateBinaryFile(string const & filePath)
{
  return fstream(filePath, ios::in|ios::out|ios::binary|ios::trunc);
}

inline fstream OpenBinaryFile(string const & filePath)
{
  return fstream(filePath, ios::in|ios::out|ios::binary);
}

} // namespace

GpsTrackFile::GpsTrackFile(string const & filePath, size_t maxItemCount)
  : m_filePath(filePath)
  , m_maxItemCount(max(maxItemCount + 1, size_t(2)))
{
  m_header.m_maxItemCount = m_maxItemCount;

  m_stream = OpenBinaryFile(filePath);
  if (!m_stream)
  {
    m_stream = CreateBinaryFile(filePath);
    CHECK((bool)m_stream, ());

    WriteHeader(m_header);
  }
  else if (!ReadHeader(m_header) || m_header.m_maxItemCount != m_maxItemCount)
  {
    // Corrupted file, rewrite it

    m_stream = CreateBinaryFile(filePath);
    CHECK((bool)m_stream, ());

    m_header = Header();
    m_header.m_maxItemCount = m_maxItemCount;

    WriteHeader(m_header);
  }
}

void GpsTrackFile::Flush()
{
  ASSERT(m_stream.is_open(), ("File is not open"));

  m_stream.flush();
}

bool GpsTrackFile::IsOpen() const
{
  return m_stream.is_open();
}

void GpsTrackFile::Close()
{
  ASSERT(m_stream.is_open(), ("File is not open"));

  m_stream.close();

  m_header = Header();
  m_header.m_maxItemCount = m_maxItemCount;
}

bool GpsTrackFile::Append(double timestamp, m2::PointD const & pt, double speed)
{
  Item item;
  item.m_timestamp = timestamp;
  item.m_x = pt.x;
  item.m_y = pt.y;
  item.m_speed = speed;

  return Append(item);
}

bool GpsTrackFile::Append(Item const & item)
{
  ASSERT(m_stream.is_open(), ("File is not open"));

  if (item.m_timestamp < m_header.m_timestamp)
  {
    ASSERT(item.m_timestamp >= m_header.m_timestamp, ("Timestamp sequence is broken"));
    return false;
  }

  size_t const newLast = (m_header.m_last + 1) % m_maxItemCount;
  size_t const newFirst = (newLast == m_header.m_first) ? ((m_header.m_first + 1) % m_maxItemCount) : m_header.m_first;

  // if newFirst != m_header.m_first then element with index m_header.m_first is popped
  // element with index m_header.m_last is added

  WriteItem(m_header.m_last, item);

  m_header.m_first = newFirst;
  m_header.m_last = newLast;
  m_header.m_timestamp = item.m_timestamp;

  WriteHeader(m_header);

  return true;
}

void GpsTrackFile::ForEach(function<bool(double timestamp, m2::PointD const & pt, double speed)> const & fn)
{
  ASSERT(m_stream.is_open(), ("File is not open"));

  double prevTimestamp = 0;

  for (size_t i = m_header.m_first; i != m_header.m_last; i = (i + 1) % m_maxItemCount)
  {
    Item item;
    if (!ReadItem(i, item))
    {
      CHECK(false, ("Inconsistent file"));
      return;
    }

    CHECK(prevTimestamp < item.m_timestamp, ("Inconsistent file"));

    m2::PointD pt(item.m_x, item.m_y);
    if (!fn(item.m_timestamp, pt, item.m_speed))
      break;

    prevTimestamp = item.m_timestamp;
  }
}

void GpsTrackFile::Clear()
{
  ASSERT(m_stream.is_open(), ("File is not open"));

  Header emptyHeader;
  emptyHeader.m_maxItemCount = m_maxItemCount;

  WriteHeader(emptyHeader);

  m_header = emptyHeader;
}

bool GpsTrackFile::IsEmpty() const
{
  return m_header.m_first == m_header.m_last;
}

double GpsTrackFile::GetTimestamp() const
{
  return m_header.m_timestamp;
}

size_t GpsTrackFile::GetCount() const
{
  return (m_header.m_first <= m_header.m_last) ? (m_header.m_last - m_header.m_first) :
                                                 (m_header.m_last + m_maxItemCount - m_header.m_first);
}

bool GpsTrackFile::ReadHeader(Header & header)
{
  m_stream.seekg(0, ios::beg);
  m_stream.read(reinterpret_cast<char *>(&header), sizeof(header));
  return ((m_stream.rdstate() & ios::eofbit) == 0);
}

void GpsTrackFile::WriteHeader(Header const & header)
{
  m_stream.seekp(0, ios::beg);
  m_stream.write(reinterpret_cast<char const *>(&header), sizeof(header));
}

bool GpsTrackFile::ReadItem(size_t index, Item & item)
{
  size_t const offset = ItemOffset(index);
  m_stream.seekg(offset, ios::beg);
  m_stream.read(reinterpret_cast<char *>(&item), sizeof(item));
  return ((m_stream.rdstate() & ios::eofbit) == 0);
}

void GpsTrackFile::WriteItem(size_t index, Item const & item)
{
  size_t const offset = ItemOffset(index);
  m_stream.seekp(offset, ios::beg);
  m_stream.write(reinterpret_cast<char const *>(&item), sizeof(item));
}

size_t GpsTrackFile::ItemOffset(size_t index)
{
  return sizeof(Header) + index * sizeof(Item);
}
