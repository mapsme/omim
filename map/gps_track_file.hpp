#pragma once

#include "std/string.hpp"

#include "base/assert.hpp"

class GpsTrackFile
{
public:
  struct Item
  {
    double m_timestamp;
    double m_x;
    double m_y;
    double m_speed;
  };

  GpsTrackFile(string const & filePath, uint32_t maxPageCount)
    : m_filePath(filePath)
    , m_maxPageCount(max(2, maxPageCount))
  {
  }

  void Flush()
  {
    FlushPage(m_lastPage);
    FlushPage(m_firstPage);
    FlushHeader();
  }

  void Append(Item const & item)
  {
    if (m_lastPage->CanPushBack())
    {
      m_lastPage->PushBack(item);
      return;
    }

    FlushPage(m_lastPage);
    FlushHeader();

    uint32_t const nextLastIndex = (m_lastPage->GetIndex() + 1) % m_maxPageCount;

    if (nextLastIndex == m_firstPage->GetIndex())
    {
      // Last has reached First, new Last will be old First and new First will be shifted for one
      // FxxxL
      // LFxxx

      uint32_t const nextFirstIndex = (nextLastIndex + 1) % m_maxPageCount;

      // load next first page
      m_firstPage->SetIndex(nextFirstIndex);
      m_firstPage->Clear();
      m_firstPage->SetDirty(false);
      LoadPage(m_firstPage);
    }

    // alloc new page
    if (m_lastPage != m_firstPage)
      m_lastPage->Clear(); // Reuse page
    else
      m_lastPage = new Page(); // New page
    m_lastPage->SetIndex(newLastIndex);
    m_lastPage->SetDirty(false);

    m_lastPage->PushBack(item);
  }

  bool PopFront(Item & item)
  {
    if (m_firstPage->CanPopFront())
    {
      item = m_firstPage->PopFront();
      return true;
    }
    else
    {
      if (m_firstPage == m_lastPage)
      {
        // no more data
        return false;
      }
    }

    size_t const nextFirstIndex = (1 + m_firstPage->GetIndex()) % m_maxPageCount;

    if (nextFirstIndex == m_lastPage->GetIndex())
    {
      // next first page is the last page

      m_firstPage = m_lastPage;

      if (m_firstPage->CanPopFront())
      {
        item = m_firstPage->PopFront();
        return true;
      }

      // no more data
      return false;
    }

    // load next first page
    m_firstPage->Clear();
    m_firstPage->SetIndex(nextFirstIndex);
    m_firstPage->SetDirty(false);
    LoadPage(m_firstPage);

    if (!m_firstPage->CanPopFront())
    {
      // Error in file because next first page shoule be non-empty
      return false;
    }

    item = m_firstPage->PopFront();
    return true;
  }

private:
  void LoadPage(Page * page)
  {
    uint32_t const index = page->GetIndex();
    uint32_t const offset = index * kPageSizeBytes + sizeof(Head);
    // read page->m_first in position 'offset'
    // read page->m_last in position 'offset + sizeof(uint32_t)'
    // read page->m_items in position 'offset + 2 * sizeof(uint32_t)'
    page->SetDirty(false);
  }

  void FlushPage(Page * page)
  {
    if (!page->IsDirty())
      return;
    uint32_t const index = page->GetIndex();
    uint32_t const offset = index * kPageSizeBytes + sizeof(Head);
    // write page->m_first in position 'offset'
    // write page->m_last in position 'offset + sizeof(uint32_t)'
    // write page->m_items in position 'offset + 2 * sizeof(uint32_t)'
    page->SetDirty(false);
  }

  void FlushHeader()
  {
    uint32_t const newFirstIndex = m_firstPage->GetIndex();
    uint32_t const newLastIndex = m_lastPage->GetIndex();

    if (m_header.m_firstIndex == newFirstIndex && m_header.m_lastIndex == newLastIndex)
      return;

    m_header.m_firstIndex = newFirstIndex;
    m_header.m_lastIndex = newLastIndex;

    // write m_header in position '0'
  }

  void ReadHeader()
  {
    // read m_header in position '0'
  }

private:
  static size_t constexpr kPageSizeBytes = 1024;
  static size_t constexpr kItemsPerPage = kPageSizeBytes / sizeof(Item);

  class Page
  {
  public:
    Page()
      : m_first(0)
      , m_last(0)
      , m_dirty(false)
      , m_index(0)
    {}
    uint32_t GetIndex() const
    {
      return m_index;
    }
    void SetIndex(uint32_t index)
    {
      m_index = index;
    }
    bool IsDirty() const
    {
      return m_dirty;
    }
    void SetDirty(bool dirty)
    {
      m_dirty = dirty;
    }
    bool CanPushBack() const
    {
      return m_last < kItemsPerPage;
    }
    bool CanPopFront() const
    {
      return m_first < m_last;
    }
    bool IsDead() const
    {
      return m_first == m_last && m_last == kItemsPerPage;
    }
    bool IsEmpty() const
    {
      return m_first == m_last;
    }
    void PushBack(Item const & item) noexcept
    {
      ASSERT(m_last < kItemsPerPage, ());
      m_items[m_last] = item;
      ++m_last;
      m_dirty = true;
    }
    Item PopFront() noexcept
    {
      ASSERT(m_first < m_last, ());
      ++m_first;
      m_dirty = true;
      return m_items[m_first - 1];
    }
    size_t Size() const noexcept
    {
      return m_last - m_first;
    }
    void Clear()
    {
      m_first = 0;
      m_last = 0;
      m_dirty = true;
    }

  private:
    Item m_items[kItemsPerPage];
    size_t m_first;
    size_t m_last;
    bool m_dirty;
    uint32_t m_index;
  };

  struct Head
  {
    uint32_t m_firstIndex;
    uint32_t m_lastIndex;
  };

  Page * m_firstPage;
  Page * m_lastPage;
  Head m_header;

  string const m_filePath;
  uint32_t const m_maxPageCount;
};
