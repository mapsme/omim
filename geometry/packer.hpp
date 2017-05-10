#pragma once

#include "geometry/rect2d.hpp"

#include <list>
#include <map>
#include <functional>
#include <queue>
#include <utility>
#include <vector>


namespace m2
{
  template <typename pair_t>
  struct first_less
  {
    bool operator()(pair_t const & first, pair_t const & second)
    {
      return first.first < second.first;
    }
  };

  /// The simplest row-by-row packer.
  /// When there is no free room all the packing
  /// rect is cleared and the process is started again.
  class Packer
  {
  public:

    typedef std::function<void()> overflowFn;

    typedef std::priority_queue<std::pair<size_t, overflowFn>, std::vector<std::pair<size_t, overflowFn> >, first_less<std::pair<size_t, overflowFn> > > overflowFns;

    typedef uint32_t handle_t;
    typedef std::pair<bool, m2::RectU> find_result_t;

  private:

    unsigned m_currentX;
    unsigned m_currentY;
    unsigned m_yStep;

    unsigned m_width;
    unsigned m_height;

    overflowFns m_overflowFns;

    handle_t m_currentHandle;

    typedef std::map<handle_t, m2::RectU> rects_t;

    rects_t m_rects;

    void callOverflowFns();

    uint32_t m_maxHandle;
    uint32_t m_invalidHandle;

  public:

    Packer();

    /// create a packer on a rectangular area of (0, 0, width, height) dimensions
    Packer(unsigned width, unsigned height, uint32_t maxHandle = 0xFFFF - 1);

    /// reset the state of the packer
    void reset();

    /// add overflow handler.
    /// @param priority handlers with higher priority value are called first.
    void addOverflowFn(overflowFn fn, int priority);

    /// pack the rect with dimensions width X height on a free area.
    /// when there is no free area - find it somehow(depending on a packer implementation,
    /// the simplest one will just clear the whole rect and start the packing process again).
    handle_t pack(unsigned width, unsigned height);

    /// return free handle
    handle_t freeHandle();

    /// Does we have room to pack another rectangle?
    bool hasRoom(unsigned width, unsigned height) const;

    /// Does we have room to pack a sequence of rectangles?
    bool hasRoom(m2::PointU const * sizes, size_t cnt) const;

    /// is the handle present on the texture.
    bool isPacked(handle_t handle);

    /// find the packed area by the handle.
    find_result_t find(handle_t handle) const;

    /// remove the handle from the list of active handles.
    void remove(handle_t handle);

    /// get an invalid handle
    uint32_t invalidHandle() const;
  };
}
