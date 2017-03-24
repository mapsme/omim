#include "build_statistics.h"

#include "build_common.h"

#include "platform/platform.hpp"

#include <QDir>
#include <QStringList>

#include <exception>

namespace
{
QString GetScriptPath()
{
  QString const resourceDir = GetPlatform().ResourcesDir().c_str();
  return resourceDir + "generate_styles_override.py";
}
}  // namespace

namespace build_style
{
QString RunBuildingPhonePack(QString const & stylesFolder, QString const & targetFolder)
{
  if (!QDir(stylesFolder).exists())
    throw std::runtime_error("styles folder does not exist");

  if (!QDir(targetFolder).exists())
    throw std::runtime_error("target folder does not exist");

  QStringList params;
  params << "python" <<
         '"' + GetScriptPath() + '"' <<
         '"' + stylesFolder + '"' <<
         '"' + targetFolder + '"';
  QString const cmd = params.join(' ');
  auto const res = ExecProcess(cmd);
  return res.second;
}
}  // namespace build_style
