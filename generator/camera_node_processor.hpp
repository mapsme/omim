#pragma once

#include "generator/osm_element.hpp"

#include "routing/base/followed_polyline.hpp"
#include "routing/routing_helpers.hpp"

#include "coding/file_writer.hpp"

#include "base/control_flow.hpp"
#include "base/logging.hpp"
#include "base/string_utils.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace generator
{
class CameraNodeProcessor
{
public:
  explicit CameraNodeProcessor(std::string const & filePath) : m_fileWriter(filePath)
  {
    LOG(LINFO, ("Saving information about cameras and ways, where they lie on, to:", filePath));
  }

  template <typename Cache>
  void ProcessAndWrite(OsmElement & p, FeatureParams const & params, Cache & cache)
  {
    std::string maxSpeedStringKmPH = params.GetMetadata().Get(feature::Metadata::EType::FMD_MAXSPEED);
    if (maxSpeedStringKmPH.empty())
      maxSpeedStringKmPH = "0";

    int32_t maxSpeed;
    if (!strings::to_int(maxSpeedStringKmPH.c_str(), maxSpeed))
      ASSERT(false, ("Bad speed format:", maxSpeedStringKmPH));

    m_fileWriter.Write(reinterpret_cast<char *>(&p.lat), sizeof(p.lat));
    m_fileWriter.Write(reinterpret_cast<char *>(&p.lon), sizeof(p.lat));
    m_fileWriter.Write(reinterpret_cast<char *>(&maxSpeed), sizeof(maxSpeed));

    std::vector<uint64_t> ways;
    cache.ForEachWayByNode(p.id, [&ways](uint64_t wayId) {
      ways.push_back(wayId);
      return base::ControlFlow::Break;
    });

    auto const size = static_cast<uint32_t>(ways.size());
    m_fileWriter.Write(reinterpret_cast<const char *>(&size), sizeof(size));
    for (auto wayId : ways)
      m_fileWriter.Write(reinterpret_cast<char *>(&wayId), sizeof(wayId));
  }

private:
  FileWriter m_fileWriter;
};
}  // namespace generator
