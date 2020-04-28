#include "indexer/index_builder.hpp"

#include "indexer/features_vector.hpp"
#include "indexer/scale_index.hpp"

#include "base/logging.hpp"

#include "defines.hpp"

namespace indexer
{
bool BuildIndexFromDataFile(std::string const & dataFile, std::string const & tmpFile,
                            FeaturesTag tag)
{
  try
  {
    std::string const idxFileName(tmpFile + GEOM_INDEX_TMP_EXT);
    {
      FeaturesVectorTest features(dataFile, tag);
      FileWriter writer(idxFileName);

      BuildIndex(features.GetHeader(), features.GetVector(), writer, tmpFile);
    }

    auto const tagStr = GetIndexTag(tag);
    FilesContainerW(dataFile, FileWriter::OP_WRITE_EXISTING).Write(idxFileName, tagStr);
    FileWriter::DeleteFileX(idxFileName);
  }
  catch (Reader::Exception const & e)
  {
    LOG(LERROR, ("Error while reading file: ", e.Msg()));
    return false;
  }
  catch (Writer::Exception const & e)
  {
    LOG(LERROR, ("Error writing index file: ", e.Msg()));
    return false;
  }

  return true;
}
}  // namespace indexer
