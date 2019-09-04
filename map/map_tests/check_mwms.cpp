#include "testing/testing.hpp"

#include "map/feature_vec_model.hpp"

#include "indexer/data_header.hpp"
#include "indexer/interval_index.hpp"

#include "platform/local_country_file.hpp"
#include "platform/local_country_file_utils.hpp"
#include "platform/platform.hpp"

#include "base/logging.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include "defines.hpp"

using namespace std;

UNIT_TEST(CheckMWM_LoadAll)
{
  Platform & platform = GetPlatform();
  vector<platform::LocalCountryFile> localFiles;
  platform::FindAllLocalMapsInDirectoryAndCleanup(platform.WritableDir(), 0 /* version */,
                                                  -1 /* latestVersion */, localFiles);

  model::FeaturesFetcher m;
  m.InitClassificator();

  for (platform::LocalCountryFile const & localFile : localFiles)
  {
    LOG(LINFO, ("Found mwm:", localFile));
    try
    {
      auto p = m.RegisterMap(localFile);

      // Normally we do not load maps older than March 2016, but we keep WorldCoasts_obsolete.mwm
      // for migration tests.
      if (localFile.GetCountryName() == WORLD_COASTS_OBSOLETE_FILE_NAME)
        continue;

      TEST(p.first.IsAlive(), ());
      TEST_EQUAL(MwmSet::RegResult::Success, p.second, ());
    }
    catch (RootException const & ex)
    {
      TEST(false, ("Bad mwm file:", localFile));
    }
  }
}

UNIT_TEST(CheckMWM_GeomIndex)
{
  // Open mwm file from data path.
  FilesContainerR cont(GetPlatform().GetReader("minsk-pass.mwm"));

  // Initialize index reader section inside mwm.
  typedef ModelReaderPtr ReaderT;
  ReaderT reader = cont.GetReader(INDEX_FILE_TAG);
  ReaderSource<ReaderT> source(reader);
  VarSerialVectorReader<ReaderT> treesReader(source);

  // Make interval index objects for each scale bucket.
  vector<unique_ptr<IntervalIndex<ReaderT, uint32_t>>> scale2Index;
  for (size_t i = 0; i < treesReader.Size(); ++i)
  {
    scale2Index.emplace_back(make_unique<IntervalIndex<ReaderT, uint32_t>>(
        treesReader.SubReader(static_cast<uint32_t>(i))));
  }

  // Pass full coverage as input for test.
  uint64_t beg = 0;
  uint64_t end = static_cast<uint64_t>((1ULL << 63) - 1);

  // Count objects for each scale bucket.
  map<size_t, uint64_t> resCount;
  for (size_t i = 0; i < scale2Index.size(); ++i)
    scale2Index[i]->ForEach([i, &resCount](uint64_t, uint32_t)
    {
      ++resCount[i];
    }, beg, end);

  // Print results.
  LOG(LINFO, (resCount));
}
