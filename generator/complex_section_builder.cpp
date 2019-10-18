#include "generator/complex_section_builder.hpp"

#include "generator/hierarchy.hpp"
#include "generator/utils.hpp"

#include "indexer/complex/complex.hpp"
#include "indexer/complex/serdes.hpp"

#include "coding/files_container.hpp"

#include "base/file_name_utils.hpp"

#include "defines.hpp"

namespace generator
{
void BuildComplexSection(std::string const & mwmFilename,
                         std::string const & osmToFtIdsFilename,
                         std::string const & complecSrcFilename)
{
   OsmID2FeatureID osmToFtIds(osmToFtIdsFilename);
   FeatureGetter ftGetter(mwmFilename);

   indexer::SourceComplexesLoader loader(complecSrcFilename);
   auto const srcForest = loader.GetForest(base::GetNameFromFullPathWithoutExt(mwmFilename));
   auto const complexForest = indexer::TraformToComplexForest(srcForest, [&](auto const & entry) {
     auto const ids = osmToFtIds.GetFeatureIds(entry.m_id);
     CHECK(!ids.empty(), (entry.m_id));
     auto const ftId = hierarchy::GetIdWitBestGeom(ids, ftGetter);
     auto const ft = ftGetter.GetFeatureByIndex(ftId);
     CHECK(ft, (ftId, entry.m_id));
     return ftId;
   });

   if (complexForest.Size() == 0)
     LOG(LINFO, ("Section", COMPLEXES_FILE_TAG, "was not written. There is no data for this."));

   FilesContainerW cont(mwmFilename, FileWriter::OP_WRITE_EXISTING);
   auto writer = cont.GetWriter(COMPLEXES_FILE_TAG);
   auto const start = writer->Pos();
   indexer::ComplexSerdes::Serialize(*writer, complexForest);
   LOG(LINFO, ("Section", COMPLEXES_FILE_TAG, "was written. Total size is", writer->Pos() - start));
}
}  // namespace generator
