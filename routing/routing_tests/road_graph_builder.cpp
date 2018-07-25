#include "road_graph_builder.hpp"

#include "routing/road_graph.hpp"

#include "indexer/mwm_set.hpp"

#include "base/checked_cast.hpp"
#include "base/logging.hpp"
#include "base/macros.hpp"

#include "std/algorithm.hpp"
#include "std/shared_ptr.hpp"


using namespace routing;

namespace
{

auto constexpr kMaxSpeedKMpH = 5.0;

/// Class provides a valid instance of FeatureID for testing purposes
class TestValidFeatureIDProvider : private MwmSet
{
public:
  TestValidFeatureIDProvider()
  {
    UNUSED_VALUE(Register(platform::LocalCountryFile::MakeForTesting("0")));

    vector<shared_ptr<MwmInfo>> mwmsInfoList;
    GetMwmsInfo(mwmsInfoList);

    m_mwmInfo = mwmsInfoList[0];
  }

  FeatureID MakeFeatureID(uint32_t offset) const
  {
    return FeatureID(MwmSet::MwmId(m_mwmInfo), offset);
  }

private:
  /// @name MwmSet overrides
  //@{
  unique_ptr<MwmInfo> CreateInfo(platform::LocalCountryFile const &) const override
  {
    unique_ptr<MwmInfo> info(new MwmInfo());
    info->m_maxScale = 1;
    info->m_bordersRect = m2::RectD(0, 0, 1, 1);
    info->m_version.SetFormat(version::Format::lastFormat);
    return info;
  }
  unique_ptr<MwmValueBase> CreateValue(MwmInfo &) const override
  {
    return make_unique<MwmValueBase>();
  }
  //@}

  shared_ptr<MwmInfo> m_mwmInfo;
};

}  // namespace

namespace routing_test
{

void RoadGraphMockSource::AddRoad(RoadInfo && ri)
{
  CHECK_GREATER_OR_EQUAL(ri.m_junctions.size(), 2, ("Empty road"));
  m_roads.push_back(move(ri));
}

IRoadGraph::RoadInfo RoadGraphMockSource::GetRoadInfo(FeatureID const & featureId) const
{
  CHECK_LESS(featureId.m_index, m_roads.size(), ("Invalid feature id."));
  return m_roads[featureId.m_index];
}

double RoadGraphMockSource::GetSpeedKMpH(FeatureID const & featureId) const
{
  CHECK_LESS(featureId.m_index, m_roads.size(), ("Invalid feature id."));
  return m_roads[featureId.m_index].m_speedKMPH;
}

double RoadGraphMockSource::GetMaxSpeedKMpH() const { return kMaxSpeedKMpH; }

void RoadGraphMockSource::ForEachFeatureClosestToCross(m2::PointD const & /* cross */,
                                                       ICrossEdgesLoader & edgesLoader) const
{
  for (size_t roadId = 0; roadId < m_roads.size(); ++roadId)
    edgesLoader(MakeTestFeatureID(base::checked_cast<uint32_t>(roadId)), m_roads[roadId]);
}

void RoadGraphMockSource::FindClosestEdges(m2::PointD const & point, uint32_t count,
                                           vector<pair<Edge, Junction>> & vicinities) const
{
  UNUSED_VALUE(point);
  UNUSED_VALUE(count);
  UNUSED_VALUE(vicinities);
}

void RoadGraphMockSource::GetFeatureTypes(FeatureID const & featureId, feature::TypesHolder & types) const
{
  UNUSED_VALUE(featureId);
  UNUSED_VALUE(types);
}

void RoadGraphMockSource::GetJunctionTypes(Junction const & junction, feature::TypesHolder & types) const
{
  UNUSED_VALUE(junction);
  UNUSED_VALUE(types);
}

IRoadGraph::Mode RoadGraphMockSource::GetMode() const
{
  return IRoadGraph::Mode::IgnoreOnewayTag;
}

FeatureID MakeTestFeatureID(uint32_t offset)
{
  static TestValidFeatureIDProvider instance;
  return instance.MakeFeatureID(offset);
}

void InitRoadGraphMockSourceWithTest1(RoadGraphMockSource & src)
{
  IRoadGraph::RoadInfo ri0;
  ri0.m_bidirectional = true;
  ri0.m_speedKMPH = kMaxSpeedKMpH;
  ri0.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(0, 0)));
  ri0.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(5, 0)));
  ri0.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(10, 0)));
  ri0.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(15, 0)));
  ri0.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(20, 0)));

  IRoadGraph::RoadInfo ri1;
  ri1.m_bidirectional = true;
  ri1.m_speedKMPH = kMaxSpeedKMpH;
  ri1.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(10, -10)));
  ri1.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(10, -5)));
  ri1.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(10, 0)));
  ri1.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(10, 5)));
  ri1.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(10, 10)));

  IRoadGraph::RoadInfo ri2;
  ri2.m_bidirectional = true;
  ri2.m_speedKMPH = kMaxSpeedKMpH;
  ri2.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(15, -5)));
  ri2.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(15, 0)));

  IRoadGraph::RoadInfo ri3;
  ri3.m_bidirectional = true;
  ri3.m_speedKMPH = kMaxSpeedKMpH;
  ri3.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(20, 0)));
  ri3.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(25, 5)));
  ri3.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(15, 5)));
  ri3.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(20, 0)));

  src.AddRoad(move(ri0));
  src.AddRoad(move(ri1));
  src.AddRoad(move(ri2));
  src.AddRoad(move(ri3));
}

void InitRoadGraphMockSourceWithTest2(RoadGraphMockSource & graph)
{
  IRoadGraph::RoadInfo ri0;
  ri0.m_bidirectional = true;
  ri0.m_speedKMPH = kMaxSpeedKMpH;
  ri0.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(0, 0)));
  ri0.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(10, 0)));
  ri0.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(25, 0)));
  ri0.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(35, 0)));
  ri0.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(70, 0)));
  ri0.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(80, 0)));

  IRoadGraph::RoadInfo ri1;
  ri1.m_bidirectional = true;
  ri1.m_speedKMPH = kMaxSpeedKMpH;
  ri1.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(0, 0)));
  ri1.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(5, 10)));
  ri1.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(5, 40)));

  IRoadGraph::RoadInfo ri2;
  ri2.m_bidirectional = true;
  ri2.m_speedKMPH = kMaxSpeedKMpH;
  ri2.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(12, 25)));
  ri2.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(10, 10)));
  ri2.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(10, 0)));

  IRoadGraph::RoadInfo ri3;
  ri3.m_bidirectional = true;
  ri3.m_speedKMPH = kMaxSpeedKMpH;
  ri3.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(5, 10)));
  ri3.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(10, 10)));
  ri3.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(70, 10)));
  ri3.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(80, 10)));

  IRoadGraph::RoadInfo ri4;
  ri4.m_bidirectional = true;
  ri4.m_speedKMPH = kMaxSpeedKMpH;
  ri4.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(25, 0)));
  ri4.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(27, 25)));

  IRoadGraph::RoadInfo ri5;
  ri5.m_bidirectional = true;
  ri5.m_speedKMPH = kMaxSpeedKMpH;
  ri5.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(35, 0)));
  ri5.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(37, 30)));
  ri5.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(70, 30)));
  ri5.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(80, 30)));

  IRoadGraph::RoadInfo ri6;
  ri6.m_bidirectional = true;
  ri6.m_speedKMPH = kMaxSpeedKMpH;
  ri6.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(70, 0)));
  ri6.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(70, 10)));
  ri6.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(70, 30)));

  IRoadGraph::RoadInfo ri7;
  ri7.m_bidirectional = true;
  ri7.m_speedKMPH = kMaxSpeedKMpH;
  ri7.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(39, 55)));
  ri7.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(80, 55)));

  IRoadGraph::RoadInfo ri8;
  ri8.m_bidirectional = true;
  ri8.m_speedKMPH = kMaxSpeedKMpH;
  ri8.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(5, 40)));
  ri8.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(18, 55)));
  ri8.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(39, 55)));
  ri8.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(37, 30)));
  ri8.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(27, 25)));
  ri8.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(12, 25)));
  ri8.m_junctions.push_back(MakeJunctionForTesting(m2::PointD(5, 40)));

  graph.AddRoad(move(ri0));
  graph.AddRoad(move(ri1));
  graph.AddRoad(move(ri2));
  graph.AddRoad(move(ri3));
  graph.AddRoad(move(ri4));
  graph.AddRoad(move(ri5));
  graph.AddRoad(move(ri6));
  graph.AddRoad(move(ri7));
  graph.AddRoad(move(ri8));
}

}  // namespace routing_test
