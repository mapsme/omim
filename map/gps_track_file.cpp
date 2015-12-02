#include "map/gps_track_file.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"

namespace
{

size_t constexpr kLinearSearchMinDistance = 10;
size_t constexpr kMinItemCount = 2;

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
  , m_maxItemCount(max(maxItemCount, kMinItemCount) + 1)
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

  // TODO: notify about added and removed elements
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
    CHECK(ReadItem(i, item), ("Inconsistent file"));

    CHECK(prevTimestamp < item.m_timestamp, ("Inconsistent file"));

    m2::PointD pt(item.m_x, item.m_y);
    if (!fn(item.m_timestamp, pt, item.m_speed))
      break;

    prevTimestamp = item.m_timestamp;
  }
}

void GpsTrackFile::DropEarlierThan(double timestamp)
{
  ASSERT(m_stream.is_open(), ("File is not open"));

  if (IsEmpty())
    return;

  if (m_header.m_timestamp <= timestamp)
  {
    // TODO: notify about all elements were removed

    Clear();
    return;
  }

  size_t const n = GetCount();

  // Try linear search for short distance
  // In common case elements will be removed from the tail by small pieces
  size_t const linearSearchCount = min(kLinearSearchMinDistance, n);
  for (size_t i = m_header.m_first, j = 0; i != m_header.m_last; i = (i + 1) % m_maxItemCount, ++j)
  {
    if (j >= linearSearchCount)
      break;

    Item item;
    CHECK(ReadItem(i, item), ("Inconsistent file"));

    if (item.m_timestamp >= timestamp)
    {
      if (i != m_header.m_first)
      {
        // TODO: Items from range [m_header.m_first to i) are removed, notify about this

        m_header.m_first = i;
        WriteHeader(m_header);
      }

      return;
    }
  }

  // By nature items are sorted by timestamp.
  // Use lower_bound algorithm to find first element later than timestamp.
  size_t len = n, first = m_header.m_first;
  while (len > 0)
  {
    size_t const step = len / 2;
    size_t const index = (first + step) % m_maxItemCount;
    Item item;
    CHECK(ReadItem(index, item), ("Inconsistent file"));
    if (item.m_timestamp < timestamp)
    {
      first = (index + 1) % m_maxItemCount;
      len -= step + 1;
    }
    else
      len = step;
  }
  if (first != m_header.m_first)
  {
    // TODO: Items in range [m_header.m_first ... first) are removed, notify about this

    m_header.m_first = first;
    WriteHeader(m_header);
  }
}

void GpsTrackFile::Clear()
{
  ASSERT(m_stream.is_open(), ("File is not open"));

  // TODO: notify about all elements were removed

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
