#include "storage/storage_tests/test_map_files_downloader.hpp"

#include <string>
#include <vector>

namespace storage
{
void TestMapFilesDownloader::GetServersList(int64_t const mapVersion, std::string const & mapFileName,
                                            TServersListCallback const & callback)
{
  std::vector<std::string> urls = {"http://localhost:34568/unit_tests/"};
  callback(urls);
}
}  // namespace storage
