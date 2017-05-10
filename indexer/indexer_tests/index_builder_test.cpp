#include "testing/testing.hpp"

#include "indexer/index.hpp"
#include "indexer/index_builder.hpp"
#include "indexer/classificator_loader.hpp"
#include "indexer/features_vector.hpp"
#include "indexer/scales.hpp"

#include "defines.hpp"

#include "platform/platform.hpp"

#include "coding/file_container.hpp"

#include "base/macros.hpp"
#include "base/stl_add.hpp"


UNIT_TEST(BuildIndexTest)
{
  Platform & p = GetPlatform();
  classificator::Load();

  FilesContainerR originalContainer(p.GetReader("minsk-pass" DATA_FILE_EXTENSION));

  // Build index.
  vector<char> serialIndex;
  {
    FeaturesVectorTest features(originalContainer);

    MemWriter<vector<char> > serialWriter(serialIndex);
    indexer::BuildIndex(features.GetHeader(), features.GetVector(), serialWriter, "build_index_test");
  }

  // Create a new mwm file.
  std::string const fileName = "build_index_test" DATA_FILE_EXTENSION;
  std::string const filePath = p.WritablePathForFile(fileName);
  FileWriter::DeleteFileX(filePath);

  // Copy original mwm file and replace index in it.
  {
    FilesContainerW containerWriter(filePath);
    vector<std::string> tags;
    originalContainer.ForEachTag(MakeBackInsertFunctor(tags));
    for (size_t i = 0; i < tags.size(); ++i)
    {
      if (tags[i] != INDEX_FILE_TAG)
        containerWriter.Write(originalContainer.GetReader(tags[i]), tags[i]);
    }
    containerWriter.Write(serialIndex, INDEX_FILE_TAG);
  }

  {
    // Check that index actually works.
    Index index;
    UNUSED_VALUE(index.Register(platform::LocalCountryFile::MakeForTesting("build_index_test")));

    // Make sure that index is actually parsed.
    NoopFunctor fn;
    index.ForEachInScale(fn, 15);
  }

  // Clean after the test.
  FileWriter::DeleteFileX(filePath);
}
