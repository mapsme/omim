#include "map/gps_track_file.hpp"

#include "std/algorithm.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"

namespace
{
size_t constexpr kBlockSize = 1000;
}

GpsTrackFile::GpsTrackFile()
  : m_maxItemCount(0)
  , m_itemCount(0)
{
}

GpsTrackFile::~GpsTrackFile()
{
}

void GpsTrackFile::Open(string const & filePath, size_t maxItemCount)
{
  ASSERT(!IsOpen(), ());
  ASSERT(maxItemCount > 0, ());

  m_file.reset(new my::FileData(filePath, my::FileData::OP_WRITE_READ_OPEN_ALWAYS));

  size_t const fileSize = m_file->Size();

  m_itemCount = fileSize / sizeof(TItem);
  m_maxItemCount = maxItemCount;

  // Set write position after the last item
  m_file->Seek(m_itemCount * sizeof(TItem));
}

void GpsTrackFile::Create(string const & filePath, size_t maxItemCount)
{
  ASSERT(!IsOpen(), ());
  ASSERT(maxItemCount > 0, ());

  m_file.reset(new my::FileData(filePath, my::FileData::OP_WRITE_READ_OPEN_ALWAYS));
  m_file->Truncate(0);

  m_itemCount = 0;
  m_maxItemCount = maxItemCount;

  // Write position is set to the begin of file
}

bool GpsTrackFile::IsOpen() const
{
  return m_file != nullptr;
}

void GpsTrackFile::Close()
{
  ASSERT(IsOpen(), ());

  m_file->Flush();
  m_file.reset();
  m_itemCount = 0;
  m_maxItemCount = 0;
}

void GpsTrackFile::Flush()
{
  ASSERT(IsOpen(), ());

  m_file->Flush();
}

void GpsTrackFile::Append(vector<TItem> const & items)
{
  ASSERT(IsOpen(), ());

  bool const needTrunc = (m_itemCount + items.size()) > (m_maxItemCount * 2);

  if (needTrunc)
    TruncFile();

  // Write position must be after last item position
  ASSERT_EQUAL(m_file->Pos(), m_itemCount * sizeof(TItem), ());

  m_file->Write(&items[0], items.size() * sizeof(TItem));

  m_itemCount += items.size();
}

void GpsTrackFile::Clear()
{
  ASSERT(IsOpen(), ());

  m_file->Truncate(0);

  m_itemCount = 0;

  // Write position is set to the begin of file
}

void GpsTrackFile::ForEach(std::function<bool(TItem const & item)> const & fn)
{
  ASSERT(IsOpen(), ());

  vector<TItem> items(kBlockSize);

  size_t i = GetFirstItemIndex();
  for (bool stop = false; i < m_itemCount && !stop;)
  {
    size_t const n = min(m_itemCount - i, items.size());
    m_file->Read(i * sizeof(TItem), &items[0], n * sizeof(TItem));

    for (size_t j = 0; j < n; ++j)
    {
      TItem const & item = items[j];
      if (!fn(item))
      {
        stop = true;
        break;
      }
    }

    i += n;
  }

  // Set write position after the last item
  m_file->Seek(m_itemCount * sizeof(TItem));
}

void GpsTrackFile::TruncFile()
{
  if (m_itemCount <= m_maxItemCount)
    return;

  vector<TItem> items(kBlockSize);

  size_t i = GetFirstItemIndex();

  size_t j = 0;
  for (; i < m_itemCount;)
  {
    size_t const n = min(m_itemCount - i, items.size());
    m_file->Read(i * sizeof(TItem), &items[0], n * sizeof(TItem));
    m_file->Seek(j * sizeof(TItem));
    m_file->Write(&items[0], n * sizeof(TItem));
    i += n;
    j += n;
  }

  m_file->Truncate(j * sizeof(TItem));

  // Set write position after the last item
  m_file->Seek(j * sizeof(TItem));

  m_itemCount = j;
}

size_t GpsTrackFile::GetFirstItemIndex() const
{
  return (m_itemCount > m_maxItemCount) ? (m_itemCount - m_maxItemCount) : 0;
}
