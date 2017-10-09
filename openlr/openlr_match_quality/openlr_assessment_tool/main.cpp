#include "map/framework.hpp"

#include "openlr/openlr_match_quality/openlr_assessment_tool/mainwindow.hpp"

#include "3party/gflags/src/gflags/gflags.h"

#include <cstdio>

#include <QApplication>

using namespace openlr;

namespace
{
DEFINE_string(resources_path, "", "Path to resources directory");
DEFINE_string(data_path, "", "Path to data directory");
DEFINE_string(login, "", "Login string");
DEFINE_string(password, "", "Password string");
DEFINE_string(url, "", "Url to a partner map");

bool ValidateStringFlag(char const * flagName, std::string const & val)
{
  if (!val.empty())
    return true;

  fprintf(stderr, "%1$s cannot be empty. Please specify a proper %1$s\n", flagName);
  return false;
}
}  // namespace

int main(int argc, char * argv[])
{
  ::google::RegisterFlagValidator(&FLAGS_login, &ValidateStringFlag);
  ::google::RegisterFlagValidator(&FLAGS_password, &ValidateStringFlag);
  ::google::RegisterFlagValidator(&FLAGS_url, &ValidateStringFlag);

  google::SetUsageMessage("Visualize and check matched routes.");
  google::ParseCommandLineFlags(&argc, &argv, true);

  Platform & platform = GetPlatform();
  if (!FLAGS_resources_path.empty())
    platform.SetResourceDir(FLAGS_resources_path);
  if (!FLAGS_data_path.empty())
    platform.SetWritableDirForTests(FLAGS_data_path);

  Q_INIT_RESOURCE(resources_common);
  QApplication app(argc, argv);

  FrameworkParams params;
  params.m_enableLocalAds = false;

  Framework framework(params);
  MainWindow mainWindow(framework, FLAGS_url, FLAGS_login, FLAGS_password);

  mainWindow.showMaximized();

  return app.exec();
}
