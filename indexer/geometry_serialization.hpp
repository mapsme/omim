#pragma once

#include "geometry_coding.hpp"
#include "tesselator_decl.hpp"

#include "../geometry/point2d.hpp"

#include "../coding/reader.hpp"
#include "../coding/writer.hpp"
#include "../coding/varint.hpp"

#include "../std/algorithm.hpp"
#include "../std/bind.hpp"

#include "../base/buffer_vector.hpp"
#include "../base/stl_add.hpp"


namespace serial
{
  template <class TCont, class TSink>
  inline void WriteVarUintArray(TCont const & v, TSink & sink)
  {
    for (size_t i = 0; i != v.size(); ++i)
      WriteVarUint(sink, v[i]);
  }

  namespace pts { m2::PointU D2U(m2::PointD const & p); }

  /// @name Encode and Decode function types.
  //@{
  typedef void (*EncodeFunT)( geo_coding::InPointsT const &,
                              m2::PointU const &, m2::PointU const &,
                              geo_coding::OutDeltasT &);
  typedef void (*DecodeFunT)( geo_coding::InDeltasT const &,
                              m2::PointU const &, m2::PointU const &,
                              geo_coding::OutPointsT &);
  //@}

  typedef buffer_vector<uint64_t, 32> DeltasT;
  typedef buffer_vector<m2::PointD, 32> OutPointsT;

  void Encode(EncodeFunT fn, vector<m2::PointD> const & points, int64_t base, DeltasT & deltas);

  /// @name Overloads for different out container types.
  //@{
  void Decode(DecodeFunT fn, DeltasT const & deltas, int64_t base, OutPointsT & points, size_t reserveF = 1);
  void Decode(DecodeFunT fn, DeltasT const & deltas, int64_t base, vector<m2::PointD> & points, size_t reserveF = 1);
  //@}

  template <class TSink>
  void SaveInner(EncodeFunT fn, vector<m2::PointD> const & points, int64_t base, TSink & sink)
  {
    DeltasT deltas;
    Encode(fn, points, base, deltas);
    WriteVarUintArray(deltas, sink);
  }

  template <class TSink>
  void WriteBufferToSink(vector<char> const & buffer, TSink & sink)
  {
    uint32_t const count = buffer.size();
    WriteVarUint(sink, count);
    sink.Write(&buffer[0], count);
  }

  template <class TSink>
  void SaveOuter(EncodeFunT fn, vector<m2::PointD> const & points, int64_t base, TSink & sink)
  {
    DeltasT deltas;
    Encode(fn, points, base, deltas);

    vector<char> buffer;
    MemWriter<vector<char> > writer(buffer);
    WriteVarUintArray(deltas, writer);

    WriteBufferToSink(buffer, sink);
  }

  void const * LoadInner(DecodeFunT fn, void const * pBeg, size_t count, int64_t base, OutPointsT & points);

  template <class TSource, class TPoints>
  void LoadOuter(DecodeFunT fn, TSource & src, int64_t base, TPoints & points, size_t reserveF = 1)
  {
    uint32_t const count = ReadVarUint<uint32_t>(src);
    vector<char> buffer(count);
    char * p = &buffer[0];
    src.Read(p, count);

    DeltasT deltas;
    deltas.reserve(count / 2);
    ReadVarUint64Array(p, p + count, MakeBackInsertFunctor(deltas));

    Decode(fn, deltas, base, points, reserveF);
  }


  /// @name Paths.
  //@{
  template <class TSink>
  void SaveInnerPath(vector<m2::PointD> const & points, int64_t base, TSink & sink)
  {
    SaveInner(&geo_coding::EncodePolyline, points, base, sink);
  }
  template <class TSink>
  void SaveOuterPath(vector<m2::PointD> const & points, int64_t base, TSink & sink)
  {
    SaveOuter(&geo_coding::EncodePolyline, points, base, sink);
  }

  inline void const * LoadInnerPath(void const * pBeg, size_t count, int64_t base, OutPointsT & points)
  {
    return LoadInner(&geo_coding::DecodePolyline, pBeg, count, base, points);
  }

  template <class TSource, class TPoints>
  void LoadOuterPath(TSource & src, int64_t base, TPoints & points)
  {
    LoadOuter(&geo_coding::DecodePolyline, src, base, points);
  }
  //@}

  /// @name Triangles.
  //@{
  template <class TSink>
  void SaveInnerTriangles(vector<m2::PointD> const & points, int64_t base, TSink & sink)
  {
    SaveInner(&geo_coding::EncodeTriangleStrip, points, base, sink);
  }

  inline void const * LoadInnerTriangles(void const * pBeg, size_t count, int64_t base, OutPointsT & points)
  {
    return LoadInner(&geo_coding::DecodeTriangleStrip, pBeg, count, base, points);
  }

  class TrianglesChainSaver
  {
    typedef m2::PointU PointT;
    typedef tesselator::Edge EdgeT;
    typedef vector<char> BufferT;

    PointT m_base, m_max;

    list<BufferT> m_buffers;

  public:
    TrianglesChainSaver(int64_t base);

    PointT GetBasePoint() const { return m_base; }
    PointT GetMaxPoint() const { return m_max; }

    void operator() (PointT arr[3], vector<EdgeT> edges);

    size_t GetBufferSize() const
    {
      size_t sz = 0;
      for (list<BufferT>::const_iterator i = m_buffers.begin(); i != m_buffers.end(); ++i)
        sz += i->size();
      return sz;
    }

    template <class TSink> void Save(TSink & sink)
    {
      // assume that 2 byte is enough for triangles count
      size_t const count = m_buffers.size();
      CHECK_LESS_OR_EQUAL(count, 0x3FFF, ());

      WriteVarUint(sink, static_cast<uint32_t>(count));

      for_each(m_buffers.begin(), m_buffers.end(), bind(&WriteBufferToSink<TSink>, _1, ref(sink)));
    }
  };

  void DecodeTriangles(geo_coding::InDeltasT const & deltas,
                      m2::PointU const & basePoint,
                      m2::PointU const & maxPoint,
                      geo_coding::OutPointsT & triangles);

  template <class TSource>
  void LoadOuterTriangles(TSource & src, int64_t base, OutPointsT & triangles)
  {
    int const count = ReadVarUint<uint32_t>(src);

    for (int i = 0; i < count; ++i)
      LoadOuter(&DecodeTriangles, src, base, triangles, 3);
  }
  //@}
}
