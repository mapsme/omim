#pragma once

#include "coding/reader.hpp"

#include "std/shared_ptr.hpp"

/// @TODO Add Windows support
class MmapReader : public ModelReader
{
  typedef ModelReader base_type;

  class MmapData;
  shared_ptr<MmapData> m_data;
  uint64_t m_offset;
  uint64_t m_size;

  MmapReader(MmapReader const & reader, uint64_t offset, uint64_t size);

public:
  explicit MmapReader(string const & fileName);

  virtual uint64_t Size() const;
  virtual void Read(uint64_t pos, void * p, size_t size) const;
  virtual MmapReader * CreateSubReader(uint64_t pos, uint64_t size) const;

  /// Direct file/memory access
  uint8_t * Data() const;

protected:
  // Used in special derived readers.
  void SetOffsetAndSize(uint64_t offset, uint64_t size);
};
