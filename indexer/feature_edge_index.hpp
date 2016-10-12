#pragma once

#include "coding/bit_streams.hpp"
#include "coding/elias_coder.hpp"

#include "geometry/point2d.hpp"

#include "coding/reader.hpp"
#include "coding/write_to_sink.hpp"

#include "defines.hpp"

#include "base/stl_helpers.hpp"

#include "std/algorithm.hpp"
#include "std/cstdint.hpp"
#include "std/functional.hpp"
#include "std/limits.hpp"
#include "std/vector.hpp"

namespace feature
{
uint32_t constexpr kFixPointFactor = 100000;
uint32_t constexpr kInvalidFeatureId = numeric_limits<uint32_t>::max();

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
  OutgoingEdge() = default;
  OutgoingEdge(m2::PointI const & pointTo, uint32_t segId, bool forward)
    : m_pointTo(pointTo), m_segId(segId), m_forward(forward)
  {
  }

  m2::PointI m_pointTo;
  uint32_t m_segId = 0;
  bool m_forward = true;
};

inline bool operator==(OutgoingEdge const & lhv, OutgoingEdge const & rhv)
{
  return lhv.m_pointTo == rhv.m_pointTo && lhv.m_segId == rhv.m_segId && lhv.m_forward == rhv.m_forward;
}

struct PointOutgoingEdges
{
  explicit PointOutgoingEdges(m2::PointI const & pointFrom) : m_pointFrom(pointFrom) {}
  PointOutgoingEdges() = default;

  m2::PointI m_pointFrom;
  vector<OutgoingEdge> m_edges;
};

inline bool operator==(PointOutgoingEdges const & lhv, PointOutgoingEdges const & rhv)
{
  return lhv.m_pointFrom == rhv.m_pointFrom && lhv.m_edges == rhv.m_edges;
}

auto const outgoingEdgeSortFunc = my::LessBy(&PointOutgoingEdges::m_pointFrom);

template <class TSink>
void EncodePointI(m2::PointI const & point, BitWriter<TSink> & bits)
{
  auto const deltaX = bits::ZigZagEncode(point.x);
  auto const deltaY = bits::ZigZagEncode(point.y);
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

  return m2::PointI(bits::ZigZagDecode(deltaX), bits::ZigZagDecode(deltaY));
}

template <class TSink>
void EncodeBool(bool flag, BitWriter<TSink> & bits)
{
  coding::DeltaCoder::Encode(bits, flag ? 2 /* true */ : 1 /* false */);
}

template <class TSource>
bool DecodeBool(BitReader<TSource> & bits)
{
  auto const flag = coding::DeltaCoder::Decode(bits);
  return flag == 1 ? false : true;
}

template <class TSink>
void EncodeUint(uint32_t i, BitWriter<TSink> & bits)
{
  coding::DeltaCoder::Encode(bits, i + 1 /* making it greater than zero */);
}

template <class TSource>
uint32_t DecodeUint(BitReader<TSource> & bits)
{
  auto const i = coding::DeltaCoder::Decode(bits);
  if (i == 0)
  {
    ASSERT(false, ());
    return 0;
  }
  return i - 1;
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
    CHECK_LESS_OR_EQUAL(prevFeatureId, m_featureId, ());
    CHECK(IsSorted(), ());

    BitWriter<TSink> bits(sink);
    coding::DeltaCoder::Encode(bits, m_featureId - prevFeatureId + 1 /* making it greater than zero */);
    coding::DeltaCoder::Encode(bits, m_featureOutgoingEdges.size());

    m2::PointI prevPointFrom = mwmCenter;
    for(auto const & pointOutgoingEdges : m_featureOutgoingEdges)
    {
      // Start point outgoing edges.
      EncodePointI(pointOutgoingEdges.m_pointFrom - prevPointFrom, bits);
      // Number of outgoing edges.
      coding::DeltaCoder::Encode(bits,
                                 pointOutgoingEdges.m_edges.size() + 1 /* making it greater than zero */);
      for(auto const & edge : pointOutgoingEdges.m_edges)
      {
        // End point of an outgoing edge.
        EncodePointI(edge.m_pointTo - pointOutgoingEdges.m_pointFrom, bits);
        EncodeUint(edge.m_segId, bits);
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
    uint64_t const decodedFeatrueIdPlus1 = prevFeatureId + coding::DeltaCoder::Decode(bits);
    CHECK_LESS(0, decodedFeatrueIdPlus1, ());
    m_featureId = decodedFeatrueIdPlus1 - 1;
    size_t const featurePointCount = coding::DeltaCoder::Decode(bits);
    m2::PointI prevPointFrom = mwmCenter;
    for (size_t i = 0; i < featurePointCount; ++i)
    {
      PointOutgoingEdges edges;
      edges.m_pointFrom = DecodePointI(bits) + prevPointFrom;
      uint32_t outgoingEdgeCount = coding::DeltaCoder::Decode(bits);
      CHECK_LESS(0, outgoingEdgeCount, ());
      outgoingEdgeCount -= 1;
      for (uint32_t j = 0; j < outgoingEdgeCount; ++j)
      {
        OutgoingEdge edge;
        edge.m_pointTo = edges.m_pointFrom + DecodePointI(bits);
        edge.m_segId = DecodeUint(bits);
        edge.m_forward = DecodeBool(bits);
        edges.m_edges.push_back(move(edge));
      }

      m_featureOutgoingEdges.push_back(move(edges));
      prevPointFrom = edges.m_pointFrom;
    }
    CHECK(IsSorted(), ("Wrong", EDGE_INDEX_FILE_TAG, "section. Feature outgoing edges "
                       "are saved in wrong format."));
  }

  void SortUnique()
  {
    my::SortUnique(m_featureOutgoingEdges, outgoingEdgeSortFunc, equal_to<PointOutgoingEdges>());
  }

  bool IsSorted() const
  {
    return is_sorted(m_featureOutgoingEdges.begin(), m_featureOutgoingEdges.end(), outgoingEdgeSortFunc);
  }

  uint32_t m_featureId;
  vector<PointOutgoingEdges> m_featureOutgoingEdges;
};
}  // namespace feature
