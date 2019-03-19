#include "routing/restriction_loader.hpp"

#include "routing/restrictions_serialization.hpp"
#include "routing/road_index.hpp"

#include "indexer/mwm_set.hpp"

#include "base/stl_helpers.hpp"

#include <iterator>

namespace
{
using namespace routing;

/// \returns if features |r1| and |r2| have a common end returns its joint id.
/// If not, returns Joint::kInvalidId.
/// \note It's possible that the both ends of |r1| and |r2| have common joint ids.
/// In that case returns any of them.
/// \note In general case ends of features don't have to be joints. For example all
/// loose feature ends aren't joints. But if ends of r1 and r2 are connected at this
/// point there has to be a joint. So the method is valid.
Joint::Id GetCommonEndJoint(RoadJointIds const & r1, RoadJointIds const & r2)
{
  auto const & j11 = r1.GetJointId(0 /* point id */);
  auto const & j12 = r1.GetEndingJointId();
  auto const & j21 = r2.GetJointId(0 /* point id */);
  auto const & j22 = r2.GetEndingJointId();

  if (j11 == j21 || j11 == j22)
    return j11;

  if (j12 == j21 || j12 == j22)
    return j12;

  return Joint::kInvalidId;
}
}  // namespace

namespace routing
{
RestrictionLoader::RestrictionLoader(MwmValue const & mwmValue, IndexGraph const & graph)
  : m_countryFileName(mwmValue.GetCountryFileName())
{
  if (!mwmValue.m_cont.IsExist(RESTRICTIONS_FILE_TAG))
    return;

  try
  {
    m_reader = make_unique<FilesContainerR::TReader>(mwmValue.m_cont.GetReader(RESTRICTIONS_FILE_TAG));
    ReaderSource<FilesContainerR::TReader> src(*m_reader);
    m_header.Deserialize(src);

    RestrictionVec restrictionsOnly;
    RestrictionSerializer::Deserialize(m_header, m_restrictions /* restriction No */,
                                       restrictionsOnly, src);
    ConvertRestrictionsOnlyToNoAndSort(graph, restrictionsOnly, m_restrictions);
  }
  catch (Reader::OpenException const & e)
  {
    m_header.Reset();
    LOG(LERROR,
        ("File", m_countryFileName, "Error while reading", RESTRICTIONS_FILE_TAG, "section.", e.Msg()));
    throw;
  }
}

void ConvertRestrictionsOnlyToNoAndSort(IndexGraph const & graph,
                                        RestrictionVec & restrictionsOnly,
                                        RestrictionVec & restrictionsNo)
{
  for (auto & restriction : restrictionsOnly)
  {
    if (restriction.size() != 2)
    {
      LOG(LINFO, ("Hello to fucking restriction mafucka"));
    }

    if (std::any_of(restriction.begin(), restriction.end(),
                    [&graph](auto const & item)
                    {
                      return !graph.IsRoad(item);
                    }))
    {
      continue;
    }

    auto const lastFeatureId     = *(restriction.end() - 1);
    auto const prevLastFeatureId = *(restriction.end() - 2);

    if (lastFeatureId == 301633 || prevLastFeatureId == 301633)
    {
      int asd = 5;
      (void)asd;
    }

    // Looking for a joint of an intersection of |o| features.
    Joint::Id const common =
      GetCommonEndJoint(graph.GetRoad(prevLastFeatureId), graph.GetRoad(lastFeatureId));

    if (common == Joint::kInvalidId)
      continue;

    std::move_iterator<std::vector<uint32_t>::iterator> start(restriction.begin());
    std::move_iterator<std::vector<uint32_t>::iterator> end(restriction.end() - 1);
    vector<uint32_t> commonVector(start, end);

    // Adding restriction of type No for all features of joint |common| except for
    // the second feature of restriction |o|.
    graph.ForEachPoint(common, [&](RoadPoint const & rp) {
      if (rp.GetFeatureId() != lastFeatureId)
      {
        std::vector<uint32_t> result(commonVector);
        result.emplace_back(rp.GetFeatureId());
        restrictionsNo.emplace_back(std::move(result));
      }
    });
  }

//  LOG(LINFO, ("restrictionsNo.size() =", restrictionsNo.size()));
  base::SortUnique(restrictionsNo);
}
}  // namespace routing
