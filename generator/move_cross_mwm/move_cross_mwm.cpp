#include "generator/routing_helpers.hpp"

#include "routing/cross_mwm_connector_serialization.hpp"

#include "coding/file_container.hpp"

#include "defines.hpp"

#include "3party/gflags/src/gflags/gflags.h"

DEFINE_string(old_mwm, "", "Path to the old mwm file.");
DEFINE_string(new_mwm, "", "Path to the new mwm file.");
DEFINE_string(osm2ft_path, "", "Path to an osm2ft file for the new mwm.");

using namespace routing;

int main(int argc, char ** argv)
{
  google::SetUsageMessage(
      "Move a crossmwm section from one mwm to another, using an osm2ft file to renumber "
      "features.");
  google::ParseCommandLineFlags(&argc, &argv, true);

  if (FLAGS_old_mwm.empty() || FLAGS_new_mwm.empty())
  {
    google::ShowUsageWithFlagsRestrict("Move Cross MWM", "move_cross_mwm");
    return -1;
  }

  string osmToFeatureFile =
      !FLAGS_osm2ft_path.empty() ? FLAGS_osm2ft_path : FLAGS_new_mwm + OSM2FEATURE_FILE_EXTENSION;
  map<osm::Id, uint32_t> osmIdToFeatureId;
  if (!ParseOsmIdToFeatureIdMapping(osmToFeatureFile, osmIdToFeatureId))
    return false;

  FilesContainerR contOld(FLAGS_old_mwm);
  FilesContainerR::TReader const reader =
      FilesContainerR::TReader(contOld.GetReader(CROSS_MWM_FILE_TAG));
  ReaderSource<FilesContainerR::TReader> src(reader);

  FilesContainerW cont(FLAGS_new_mwm, FileWriter::OP_WRITE_EXISTING);
  FileWriter writer = cont.GetWriter(CROSS_MWM_FILE_TAG);

  // Read and copy the header.
  CrossMwmConnectorSerializer::Header header;
  header.Deserialize(src);
  header.Serialize(writer);

  uint8_t const bitsPerOsmId = header.GetBitsPerOsmId();
  uint8_t const bitsPerMask = header.GetBitsPerMask();
  auto const & codingParams = header.GetCodingParams();

  // Renumber transitions.
  auto const numTransitions = base::checked_cast<size_t>(header.GetNumTransitions());
  for (size_t i = 0; i < numTransitions; ++i)
  {
    CrossMwmConnectorSerializer::Transition transition;
    transition.Deserialize(codingParams, bitsPerOsmId, bitsPerMask, src);
    auto const & osmIt = osmIdToFeatureId.find(osm::Id::Way(transition.GetOsmId()));
    bool found = osmIt != osmIdToFeatureId.cend();
    CHECK(found, ("Could not find feature Id for osm Id", transition.GetOsmId()));
    if (found)
      transition.ReplaceFeatureId(osmIt->second);
    transition.Serialize(codingParams, bitsPerOsmId, bitsPerMask, writer);
  }

  // Copy all weights from source to target.
  for (CrossMwmConnectorSerializer::Section const & section : header.GetSections())
  {
    vector<uint8_t> buffer(section.GetSize());
    src.Read(buffer.data(), buffer.size());
    writer.Write(buffer.data(), buffer.size());
  }

  return 0;
}
