#pragma once

#include "coding/bit_streams.hpp"
#include "coding/elias_coder.hpp"

#include "geometry/point2d.hpp"

#include "coding/reader.hpp"
#include "coding/write_to_sink.hpp"

#include "defines.hpp"

#include "std/algorithm.hpp"
#include "std/cstdint.hpp"
#include "std/limits.hpp"
#include "std/vector.hpp"

namespace feature
{
uint32_t constexpr kFixPointFactor = 1000000;
int32_t constexpr kInvalidLatLog = numeric_limits<int32_t>::min();

struct EdgeIndexHeader
{
  explicit EdgeIndexHeader(uint32_t featureCount = 0)
    : m_version(0), m_reserved(0), m_featureCount(featureCount)
  {
  }

  template <class TSink>
  void Serialize(TSink & sink) const
  {
    WriteToSink(sink, m_version);
    WriteToSink(sink, m_reserved);
    WriteToSink(sink, m_featureCount);
  }

  template <class TSource>
  void Deserialize(TSource & src)
  {
    m_version = ReadPrimitiveFromSource<uint16_t>(src);
    m_reserved = ReadPrimitiveFromSource<uint16_t>(src);
    m_featureCount = ReadPrimitiveFromSource<uint32_t>(src);
  }

  uint16_t m_version;
  uint16_t m_reserved;
  uint32_t m_featureCount;
};

static_assert(sizeof(EdgeIndexHeader) == 8, "Wrong header size of edge index section.");

struct OutgoingEdge
{
  m2::PointI m_pointTo;
  bool m_forward;
};

struct PointOutgoingEdges
{
  m2::PointI m_pointFrom;
  vector<OutgoingEdge> m_edges;
};

bool OutgoingEdgeSortFunc(PointOutgoingEdges const & i, PointOutgoingEdges const & j)
{
  return i.m_pointFrom < j.m_pointFrom;
}

template <class TSink>
void EncodePointI(m2::PointI const & point, BitWriter<TSink> & bits)
{
  uint32_t const deltaX = bits::ZigZagEncode(point.x);
  uint32_t const deltaY = bits::ZigZagEncode(point.y);
  coding::DeltaCoder::Encode(bits, deltaX + 1 /* making it greater than zero */);
  coding::DeltaCoder::Encode(bits, deltaY + 1 /* making it greater than zero */);
}

template <class TSource>
m2::PointI DecodePointI(BitReader<TSource> & bits)
{
  auto deltaX = coding::DeltaCoder::Decode(bits);
  auto deltaY = coding::DeltaCoder::Decode(bits);
  if (deltaX == 0 || deltaY == 0)
  {
    ASSERT(false, ());
    return m2::PointI(0, 0);
  }
  deltaX -= 1;
  deltaY -= 1;

  return m2::PointI(deltaX = bits::ZigZagDecode(deltaX), bits::ZigZagDecode(deltaY));
}

template <class TSink>
void EncodeBool(bool flag, BitWriter<TSink> & bits)
{
  coding::DeltaCoder::Encode(bits, flag ? 2 /* true */ : 1 /* false */);
}

template <class TSource>
bool DecodeBool(BitReader<TSource> & bits)
{
  auto flag =  coding::DeltaCoder::Decode(bits);
  return flag == 1 ? false : true;
}

struct FeatureOutgoingEdges
{
  explicit FeatureOutgoingEdges(uint32_t const featureId = 0) : m_featureId(featureId)
  {
  }

  template <class TSink>
  void Serialize(m2::PointI const & mwmCenter, uint32_t prevFeatureId, TSink & sink) const
  {
    CHECK(!m_featureOutgoingEdges.empty(), ());
    CHECK_LESS(prevFeatureId, m_featureId, ());
    CHECK(IsSorted(), ());

    BitWriter<TSink> bits(sink);
    coding::DeltaCoder::Encode(bits, m_featureId - prevFeatureId);
    coding::DeltaCoder::Encode(bits, m_featureOutgoingEdges.size());

    m2::PointI prevPointFrom = mwmCenter;
    for(auto const & pointOutgoingEdges : m_featureOutgoingEdges)
    {
      // Start point outgoing edges.
      EncodePointI(prevPointFrom - pointOutgoingEdges.m_pointFrom, bits);
      // Number of outgoing edges.
      coding::DeltaCoder::Encode(bits,
                                 pointOutgoingEdges.m_edges.size() + 1 /* making it greater than zero */);
      for(auto const & edge : pointOutgoingEdges.m_edges)
      {
        // End point of an outgoing edge.
        EncodePointI(pointOutgoingEdges.m_pointFrom - edge.m_pointTo, bits);
        EncodeBool(edge.m_forward, bits);
      }
      prevPointFrom = pointOutgoingEdges.m_pointFrom;
    }
  }

  // @TODO It's possible to use feature center instead of mwm center to minimize the section size.
  template <class TSource>
  void Deserialize(m2::PointI const & mwmCenter, uint32_t prevFeatureId,
                   TSource & src)
  {
    BitReader<TSource> bits(src);
    m_featureId = prevFeatureId + coding::DeltaCoder::Decode(bits);
    size_t const featurePointCount = coding::DeltaCoder::Decode(bits);
    m2::PointI prevPointFrom = mwmCenter;
    for (size_t i = 0; i < featurePointCount; ++i)
    {
      PointOutgoingEdges edges;
      edges.m_pointFrom = DecodePointI(bits) + prevPointFrom;
      uint32_t outgoingEdgeCount = coding::DeltaCoder::Decode(bits);
      if (outgoingEdgeCount == 0)
      {
        ASSERT(false, ());
        continue;
      }
      outgoingEdgeCount -= 1;
      for (uint32_t j = 0; j < outgoingEdgeCount; ++j)
      {
        OutgoingEdge edge;
        edge.m_pointTo = edges.m_pointFrom + DecodePointI(bits);
        edge.m_forward = DecodeBool(bits);
        edges.m_edges.push_back(move(edge));
      }

      m_featureOutgoingEdges.push_back(move(edges));
      prevPointFrom = edges.m_pointFrom;
    }
    CHECK(IsSorted(), ("Wrong", EDGE_INDEX_FILE_TAG, "section. Feature outgoing edges "
                       "are saved in wrong format."));
  }

  void Sort()
  {
    sort(m_featureOutgoingEdges.begin(), m_featureOutgoingEdges.end(), OutgoingEdgeSortFunc);
  }

  bool IsSorted() const
  {
    return is_sorted(m_featureOutgoingEdges.begin(), m_featureOutgoingEdges.end(), OutgoingEdgeSortFunc);
  }

  uint32_t m_featureId;
  vector<PointOutgoingEdges> m_featureOutgoingEdges;
};

}  // namespace feature
