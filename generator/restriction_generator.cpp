#include "generator/restriction_generator.hpp"

#include "generator/restriction_collector.hpp"

#include "coding/file_container.hpp"
#include "coding/file_writer.hpp"

#include "base/checked_cast.hpp"
#include "base/logging.hpp"
#include "base/stl_helpers.hpp"

#include "defines.hpp"

#include <algorithm>

namespace routing
{
std::unique_ptr<RestrictionCollector>
CreateRestrictionCollectorAndParse(
    std::string const & targetPath, std::string const & mwmPath, std::string const & country,
    std::string const & restrictionPath, std::string const & osmIdsToFeatureIdsPath,
    CountryParentNameGetterFn const & countryParentNameGetterFn)
{
  LOG(LDEBUG, ("BuildRoadRestrictions(", targetPath, ", ", restrictionPath, ", ",
                                         osmIdsToFeatureIdsPath, ");"));

  auto restrictionCollector = std::make_unique<RestrictionCollector>();
  if (!restrictionCollector->PrepareOsmIdToFeatureId(osmIdsToFeatureIdsPath))
    return {};

  try
  {
    restrictionCollector->InitIndexGraph(targetPath, mwmPath, country, countryParentNameGetterFn);
  }
  catch (std::exception const & e)
  {
    LOG(LINFO, ("Can not init IndexGraph for RestrictionCollector:", e.what()));
  }

  if (!restrictionCollector->Process(restrictionPath))
    return {};

  if (!restrictionCollector->HasRestrictions())
  {
    LOG(LINFO, ("No restrictions for", targetPath, "It's necessary to check that",
                restrictionPath, "and", osmIdsToFeatureIdsPath, "are available."));
    return {};
  }

  return restrictionCollector;
}

void SerializeRestrictions(RestrictionCollector const & restrictionCollector,
                           std::string const & mwmPath)
{
  std::vector<Restriction> const & restrictions = restrictionCollector.GetRestrictions();

  auto const firstOnlyIt =
      std::lower_bound(restrictions.cbegin(), restrictions.cend(),
                       Restriction(Restriction::Type::Only, {} /* links */),
                       base::LessBy(&Restriction::m_type));

  RestrictionHeader header;
  header.m_noRestrictionCount = base::checked_cast<uint32_t>(std::distance(restrictions.cbegin(), firstOnlyIt));
  header.m_onlyRestrictionCount = base::checked_cast<uint32_t>(restrictions.size() - header.m_noRestrictionCount);

  LOG(LINFO, ("Header info. There are", header.m_noRestrictionCount, "restrictions of type No and",
              header.m_onlyRestrictionCount, "restrictions of type Only"));

  FilesContainerW cont(mwmPath, FileWriter::OP_WRITE_EXISTING);
  FileWriter w = cont.GetWriter(RESTRICTIONS_FILE_TAG);
  header.Serialize(w);

  RestrictionSerializer::Serialize(header, restrictions.cbegin(), restrictions.cend(), w);
}

bool BuildRoadRestrictions(std::string const & targetPath,
                           std::string const & mwmPath,
                           std::string const & country,
                           std::string const & restrictionPath,
                           std::string const & osmIdsTofeatureIdsPath,
                           CountryParentNameGetterFn const & countryParentNameGetterFn)
{
  auto collector =
      CreateRestrictionCollectorAndParse(targetPath, mwmPath, country, restrictionPath,
                                         osmIdsTofeatureIdsPath, countryParentNameGetterFn);

  if (!collector)
    return false;

  SerializeRestrictions(*collector, mwmPath);
  return true;
}
}  // namespace routing
