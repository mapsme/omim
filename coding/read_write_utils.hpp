#pragma once

#include "coding/reader.hpp"
#include "coding/varint.hpp"

#include "base/buffer_vector.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

namespace rw
{
  template <class TSink>
  void Write(TSink & sink, uint32_t i)
  {
    WriteVarUint(sink, i);
  }

  template <class TSource>
  void Read(TSource & src, uint32_t & i)
  {
    i = ReadVarUint<uint32_t>(src);
  }

  template <class TSink>
  void Write(TSink & sink, std::string const & s)
  {
    uint32_t const count = static_cast<uint32_t>(s.size());
    WriteVarUint(sink, count);
    if (!s.empty())
      sink.Write(&s[0], count);
  }

  template <class TSource>
  void Read(TSource & src, std::string & s)
  {
    uint32_t const count = ReadVarUint<uint32_t>(src);
    s.resize(count);
    if (count > 0)
      src.Read(&s[0], count);
  }

  namespace impl
  {
    template <class TSink, class TCont>
    void WriteCont(TSink & sink, TCont const & v)
    {
      uint32_t const count = static_cast<uint32_t>(v.size());
      WriteVarUint(sink, count);
      for (uint32_t i = 0; i < count; ++i)
        Write(sink, v[i]);
    }

    template <class TSource, class TCont>
    void ReadCont(TSource & src, TCont & v)
    {
      uint32_t const count = ReadVarUint<uint32_t>(src);
      v.resize(count);
      for (size_t i = 0; i < count; ++i)
        Read(src, v[i]);
    }
  }

  template <class TSink, class T>
  void Write(TSink & sink, std::vector<T> const & v)
  {
    impl::WriteCont(sink, v);
  }

  template <class TSource, class T>
  void Read(TSource & src, std::vector<T> & v)
  {
    impl::ReadCont(src, v);
  }

  template <class TSink, class T, size_t N>
  void Write(TSink & sink, buffer_vector<T, N> const & v)
  {
    impl::WriteCont(sink, v);
  }

  template <class TSource, class T, size_t N>
  void Read(TSource & src, buffer_vector<T, N> & v)
  {
    impl::ReadCont(src, v);
  }

  template <class TSource, class TCont>
  void ReadVectorOfPOD(TSource & src, TCont & v)
  {
    typedef typename TCont::value_type ValueT;
    /// This assert fails on std::pair<int, int> and OsmID class.
    /// @todo Review this logic in future with new compiler abilities.
    /// https://trello.com/c/hzCc9bzN/1254-is-trivial-copy-read-write-utils-hpp
    //static_assert(std::is_trivially_copyable<ValueT>::value, "");

    uint32_t const count = ReadVarUint<uint32_t>(src);
    if (count > 0)
    {
      v.resize(count);
      src.Read(&v[0], count * sizeof(ValueT));
    }
  }

  template <class TSink, class TCont>
  void WriteVectorOfPOD(TSink & sink, TCont const & v)
  {
    typedef typename TCont::value_type ValueT;
    /// This assert fails on std::pair<int, int> and OsmID class.
    /// @todo Review this logic in future with new compiler abilities.
    /// https://trello.com/c/hzCc9bzN/1254-is-trivial-copy-read-write-utils-hpp
    //static_assert(std::is_trivially_copyable<ValueT>::value, "");

    uint32_t const count = static_cast<uint32_t>(v.size());
    WriteVarUint(sink, count);

    if (count > 0)
      sink.Write(&v[0], count * sizeof(ValueT));
  }

  template <typename Source, typename Cont,
            typename std::enable_if_t<std::is_integral<typename Cont::value_type>::value &&
                                      !(std::is_same<typename Cont::value_type, int8_t>::value ||
                                        std::is_same<typename Cont::value_type, uint8_t>::value), int> = 0>
  void ReadCollectionOfIntegral(Source & src, Cont & container)
  {
    using SizeType = typename Cont::size_type;
    using ValueType = typename Cont::value_type;

    static_assert(std::is_integral<SizeType>::value, "");

    auto size = ReadVarIntegral<SizeType>(src);
    auto inserter = std::inserter(container, std::end(container));
    while (size--)
      *inserter++ = ReadVarIntegral<ValueType>(src);
   }


  template <typename Sink, typename Cont,
            typename std::enable_if_t<std::is_integral<typename Cont::value_type>::value &&
                                      !(std::is_same<typename Cont::value_type, int8_t>::value ||
                                        std::is_same<typename Cont::value_type, uint8_t>::value), int> = 0>
  void WriteCollectionOfIntegral(Sink & sink, Cont const & container)
  {
    using SizeType = typename Cont::size_type;

    static_assert(std::is_integral<SizeType>::value, "");

    if (container.empty())
      return;

    WriteVarIntegral(sink, container.size());
    for (auto value : container)
      WriteVarIntegral(sink, value);
  }

  template <typename Source, typename Cont,
            typename std::enable_if_t<std::is_integral<typename Cont::value_type>::value &&
                                      (std::is_same<typename Cont::value_type, int8_t>::value ||
                                       std::is_same<typename Cont::value_type, uint8_t>::value), int> = 0>
  void ReadCollectionOfIntegral(Source & src, Cont & container)
  {
    using SizeType = typename Cont::size_type;
    using ValueType = typename Cont::value_type;

    static_assert(std::is_integral<SizeType>::value, "");

    auto size = ReadVarIntegral<SizeType>(src);
    auto inserter = std::inserter(container, std::end(container));
    while (size--)
      *inserter++ = ReadPrimitiveFromSource<ValueType>(src);
   }


  template <typename Sink, typename Cont,
            typename std::enable_if_t<std::is_integral<typename Cont::value_type>::value &&
                                      (std::is_same<typename Cont::value_type, int8_t>::value ||
                                       std::is_same<typename Cont::value_type, uint8_t>::value), int> = 0>
  void WriteCollectionOfIntegral(Sink & sink, Cont const & container)
  {
    using SizeType = typename Cont::size_type;

    static_assert(std::is_integral<SizeType>::value, "");

    if (container.empty())
      return;

    WriteVarIntegral(sink, container.size());
    for (auto value : container)
      WriteToSink(sink, value);
  }

  template <class ReaderT, class WriterT>
  void ReadAndWrite(ReaderT & reader, WriterT & writer, size_t bufferSize = 4*1024)
  {
    uint64_t size = reader.Size();
    std::vector<char> buffer(std::min(bufferSize, static_cast<size_t>(size)));

    while (size > 0)
    {
      size_t const curr = std::min(bufferSize, static_cast<size_t>(size));

      reader.Read(&buffer[0], curr);
      writer.Write(&buffer[0], curr);

      size -= curr;
    }
  }
}
