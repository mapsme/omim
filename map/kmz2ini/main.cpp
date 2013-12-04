#include "../../3party/gflags/src/gflags/gflags.h"

#include "../../map/framework.hpp"

#include "../../platform/platform.hpp"
#include "../../platform/settings.hpp"

#include "../../base/logging.hpp"

#include <QFile>


DEFINE_string(input, "", "Directory with KMZ files");
DEFINE_string(output, "", "ini files output directory");

int main(int argc, char ** argv)
{
  google::SetUsageMessage("settings.ini from KMZ bookmarks generator");
  google::ParseCommandLineFlags(&argc, &argv, false);
  if (FLAGS_input.empty() || FLAGS_output.empty())
  {
    google::ShowUsageWithFlagsRestrict(argv[0], "main");
    return -1;
  }

  Platform::FilesList kmzs;
  Platform::GetFilesByExt(FLAGS_input, ".kmz", kmzs);

  if (kmzs.empty())
  {
    LOG(LERROR, ("KMZ files are not found at", FLAGS_input));
    return -1;
  }

  Framework f;
  f.ClearBookmarks();

  // Should we customize it for each device?
  // iPhone5 settings
  f.m_scales.SetParams(2, 512);
  f.OnSize(640, 1136);

  for (size_t i = 0; i < kmzs.size(); ++i)
  {
    string const fullPath = FLAGS_input + "/" + kmzs[i];
    if (!f.AddBookmarksFile(fullPath))
      LOG(LWARNING, ("Can't load", fullPath));
  }

  string const srcPath = GetPlatform().SettingsPathForFile(SETTINGS_FILE_NAME);

  for (size_t ic = 0; ic < f.GetBmCategoriesCount(); ++ic)
  {
    BookmarkCategory * cat = f.GetBmCategory(ic);
    LOG(LINFO, ("Processing", cat->GetName()));
    for (size_t ib = 0; ib < cat->GetBookmarksCount(); ++ib)
    {
      Bookmark const * bmk = cat->GetBookmark(ib);
      double const scale = bmk->GetScale();
      f.ShowRectExVisibleScale(f.m_scales.GetRectForDrawScale(scale, bmk->GetOrg()));

      Settings::Set("ShouldShowAppStoreRate", false);
      Settings::Set("ShouldShowFacebookDialog", false);
      f.SaveState();

      string str;
      ReaderPtr<Reader>(GetPlatform().GetReader(SETTINGS_FILE_NAME)).ReadAsString(str);


      string const dstPath = FLAGS_output + "/" + cat->GetName() + "_" + cat->GetBookmark(ib)->GetName() + ".ini";
      if (QFile::exists(QString::fromStdString(dstPath)))
        QFile::remove(QString::fromStdString(dstPath));
      if (!QFile::copy(QString::fromStdString(srcPath), QString::fromStdString(dstPath)))
        LOG(LWARNING, ("Error copying from", srcPath, "to", dstPath));
    }
  }

  return 0;
}
