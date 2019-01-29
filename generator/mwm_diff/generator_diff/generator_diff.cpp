#include "generator/mwm_diff/diff.hpp"

#include "platform/platform.hpp"

#include "coding/file_name_utils.hpp"

#include "base/exception.hpp"
#include "base/logging.hpp"
#include "base/scope_guard.hpp"
#include "base/timer.hpp"
#include "base/thread_pool_computational.hpp"

#include "defines.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <future>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "3party/gflags/src/gflags/gflags.h"

DEFINE_string(old_path, "", "Input path with old mwms.");
DEFINE_string(new_path, "", "Input path with new mwms.");
DEFINE_string(diff_path, "", "Output path for diffs.");
DEFINE_uint64(num_threads, 1, "Number of threads.");

bool CheckInputs()
{
  if (FLAGS_old_path.empty())
  {
    LOG(LERROR, ("Input path with old mwms is not set."));
    return false;
  }

  if (FLAGS_new_path.empty())
  {
    LOG(LERROR, ("Input path with new mwms is not set."));
    return false;
  }

  if (FLAGS_diff_path.empty())
  {
    LOG(LERROR, ("Output path for diffs is not set."));
    return false;
  }

  if (FLAGS_num_threads == 0)
  {
    LOG(LERROR, ("Number of threads is wrong."));
    return false;
  }

  return true;
}

bool CheckAndGetFileNames(std::string const & oldPath, std::string const & newPath,
                          std::vector<std::string> & files)
{
  Platform::FilesList newMwms;
  Platform::GetFilesByExt(newPath, DATA_FILE_EXTENSION, newMwms);
  std::sort(std::begin(newMwms), std::end(newMwms));

  Platform::FilesList oldMwms;
  Platform::GetFilesByExt(oldPath, DATA_FILE_EXTENSION, oldMwms);
  std::sort(std::begin(oldMwms), std::end(oldMwms));
  if (newMwms != oldMwms)
  {
    LOG(LERROR, ("Old and new files do not match."));
    return false;
  }

  std::vector<std::pair<uint64_t, std::string>> mwmInfo;
  mwmInfo.reserve(newMwms.size());
  for (auto const & mwmName : newMwms)
  {
    uint64_t size;
    std::string fullPath = base::JoinPath(newPath, mwmName);
    if (!Platform::GetFileSizeByFullPath(fullPath, size))
      return false;

    mwmInfo.emplace_back(size, mwmName);
  }

  std::sort(std::begin(mwmInfo), std::end(mwmInfo), std::greater<decltype(mwmInfo[0])>());
  files.reserve(mwmInfo.size());
  for (auto && p : mwmInfo)
    files.push_back(std::move(p.second));

  return true;
}

int GeneratorDiff(int argc, char ** argv)
{
  google::ParseCommandLineFlags(&argc, &argv, true);
  if (!CheckInputs())
    return EXIT_FAILURE;

  auto const mainTimer = base::Timer();
  std::vector<std::string> files;
  if (!CheckAndGetFileNames(FLAGS_old_path, FLAGS_new_path, files))
    return EXIT_FAILURE;

  base::thread_pool::computational::ThreadPool threadPool(FLAGS_num_threads);
  std::vector<std::future<std::pair<double, std::string>>> futures;
  futures.reserve(files.size());
  for (auto const & filename : files)
  {
    auto f = threadPool.Submit([&]() {
      auto const timer = base::Timer();
      generator::mwm_diff::MakeDiff(
            base::JoinPath(FLAGS_old_path, filename),
            base::JoinPath(FLAGS_new_path, filename),
            base::JoinPath(FLAGS_diff_path, filename + DIFF_FILE_EXTENSION));

      auto const t = timer.ElapsedSeconds();
      LOG(LINFO, ("Finished generating diff for", filename, "in", t, "seconds."));
      return std::make_pair(t, filename);
    });

    futures.push_back(std::move(f));
  }

  std::vector<std::pair<double, std::string>> results;
  results.reserve(futures.size());
  for (auto & f : futures)
    results.push_back(f.get());

  auto const minmax = std::minmax_element(std::begin(results), std::end(results));
  auto const sum = std::accumulate(std::begin(results), std::end(results), 0.0, [](auto init, auto const & r) {
    return init + r.first;
  });

  if (minmax.first != std::end(results))
  {
    LOG(LINFO, ("Min time:", minmax.first->second, "-", minmax.first->first, "seconds."));
    LOG(LINFO, ("Max time:", minmax.second->second, "-", minmax.second->first, "seconds."));
  }

  LOG(LINFO, ("Sum time:", sum, "seconds."));
  LOG(LINFO, ("Average time:", sum / results.size(), "seconds."));
  LOG(LINFO, ("Total time:", mainTimer.ElapsedSeconds(), "seconds."));
  return EXIT_SUCCESS;
}

int main(int argc, char ** argv)
{
  try
  {
    return GeneratorDiff(argc, argv);
  }
  catch (RootException const & e)
  {
    LOG(LERROR, ("Unhandled core exception:", e.Msg()));
    return EXIT_FAILURE;
  }
  catch (std::exception const & e)
  {
    LOG(LERROR, ("Unhandled std exception:", e.what()));
    return EXIT_FAILURE;
  }
  catch (...)
  {
    LOG(LERROR, ("Unhandled unknown exception."));
    return EXIT_FAILURE;
  }
}
