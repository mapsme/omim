#pragma once

#include "base/assert.hpp"
#include "base/logging.hpp"

#include "std/algorithm.hpp"
#include "std/cstdint.hpp"
#include "std/limits.hpp"


namespace
{
uint8_t const kByteMask = 0xFF;
}  // namespace

static_assert(CHAR_BIT == 8, "");

template <typename TWriter>
class BitWriter
{
public:
  explicit BitWriter(TWriter & writer) : m_writer(writer), m_buf(0), m_bitsWritten(0) {}

  ~BitWriter()
  {
    try
    {
      Flush();
    }
    catch (...)
    {
      LOG(LWARNING, ("Caught an exception when flushing BitWriter."));
    }
  }
  
  // Returns the number of bits that have been sent to BitWriter,
  // including those that are in m_buf and are possibly
  // not flushed yet.
  uint64_t BitsWritten() const { return m_bitsWritten; }

  // Writes n bits starting with the least significant bit.
  // They are written one byte at a time so endianness is of no concern.
  // All the other bits except for the first n must be set to zero.
  void Write(uint8_t bits, uint8_t n)
  {
    if (n == 0)
      return;
    ASSERT_LESS_OR_EQUAL(n, CHAR_BIT, ());
    uint32_t bufferedBits = m_bitsWritten % CHAR_BIT;
    m_bitsWritten += n;
    if (n + bufferedBits > CHAR_BIT)
    {
      uint8_t b = (bits << bufferedBits) | m_buf;
      m_writer.Write(&b, 1);
      m_buf = bits >> (CHAR_BIT - bufferedBits);
    }
    else
    {
      if (bufferedBits > 0)
      {
        bits = (bits << bufferedBits) | m_buf;
        n += bufferedBits;
      }
      if (n == CHAR_BIT)
      {
        m_writer.Write(&bits, 1);
        bits = 0;
      }
      m_buf = bits;
    }
  }

  // Same as Write but accept up to 32 bits to write.
  void WriteAtMost32Bits(uint32_t bits, uint8_t n)
  {
    ASSERT_LESS_OR_EQUAL(n, 32, ());

    uint8_t constexpr kMinBits = CHAR_BIT;
    Write(static_cast<uint8_t>(bits), min(n, kMinBits));
    if (n > kMinBits)
      WriteAtMost32Bits(bits >> kMinBits, n - kMinBits);
  }

private:
  // Writes up to CHAR_BIT-1 last bits if they have not been written yet
  // and pads them with zeros.
  // This method cannot be made public because once a byte has been flushed there is no going back.
  void Flush()
  {
    if (m_bitsWritten % CHAR_BIT != 0)
      m_writer.Write(&m_buf, 1);
  }

  TWriter & m_writer;
  uint8_t m_buf;
  uint64_t m_bitsWritten;
};

template <typename TSource>
class BitReader
{
public:
  explicit BitReader(TSource & src) : m_src(src), m_bitsRead(0), m_bufferedBits(0), m_buf(0) {}

  // Returns the total number of bits read from this BitReader.
  uint64_t BitsRead() const { return m_bitsRead; }

  // Reads n bits and returns them as the least significant bits of an 8-bit number.
  // The underlying m_src is supposed to be byte-aligned (which is the
  // case when it reads from the place that was written to using BitWriter).
  // Read may use one lookahead byte.
  uint8_t Read(uint8_t n)
  {
    if (n == 0)
      return 0;
    ASSERT_LESS_OR_EQUAL(n, CHAR_BIT, ());
    m_bitsRead += n;
    uint8_t result = 0;
    if (n <= m_bufferedBits)
    {
      result = m_buf & (kByteMask >> (CHAR_BIT - n));
      m_bufferedBits -= n;
      m_buf >>= n;
    }
    else
    {
      uint8_t nextByte;
      m_src.Read(&nextByte, 1);
      uint32_t low = n - m_bufferedBits;
      result = ((nextByte & (kByteMask >> (CHAR_BIT - low))) << m_bufferedBits) | m_buf;
      m_buf = nextByte >> low;
      m_bufferedBits = CHAR_BIT - low;
    }
    return result;
  }

private:
  TSource & m_src;
  uint64_t m_bitsRead;
  uint32_t m_bufferedBits;
  uint8_t m_buf;
};
