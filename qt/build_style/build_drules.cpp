#include "build_drules.h"
#include "build_common.h"

#include "platform/platform.hpp"

#include "std/exception.hpp"

#include <fstream>
#include <streambuf>

#include "indexer/drules_city_rank_table.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>

namespace
{

QString GetKothicScript()
{
  QString const resourceDir = GetPlatform().ResourcesDir().c_str();
  return resourceDir + "kothic/src/libkomwm.py";
}

QString GetProtobufEgg()
{
  QString const resourceDir = GetPlatform().ResourcesDir().c_str();
  return resourceDir + "kothic/protobuf-2.6.1-py2.7.egg";
}

}  // namespace

namespace build_style
{

void BuildDrawingRulesImpl(QString const & mapcssFile, QString const & outputDir)
{
  QString const outputTemplate = outputDir + "drules_proto";
  QString const outputFile = outputTemplate + ".bin";

  // Caller ensures that output directory is clear
  if (QFile(outputFile).exists())
    throw runtime_error("Output directory is not clear");

  // Prepare command line
  QStringList params;
  params << "python" <<
            GetKothicScript() <<
            "-s" << mapcssFile <<
            "-o" << outputTemplate;
  QString const cmd = params.join(' ');

  // Add path to the protobuf EGG in the PROTOBUF_EGG_PATH environment variable
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert("PROTOBUF_EGG_PATH", GetProtobufEgg());

  // Run the script
  auto const res = ExecProcess(cmd, &env);

  // Script returs nothing and zero exit code if it is executed succesfully,
  if (res.first != 0 || !res.second.isEmpty())
  {
    QString msg = QString("System error ") + to_string(res.first).c_str();
    if (!res.second.isEmpty())
      msg = msg + "\n" + res.second;
    throw runtime_error(to_string(msg));
  }

  // Ensure generated files has non-zero size
  if (QFile(outputFile).size() == 0)
    throw runtime_error("Drawing rules file has zero size");

  // Copy city_rank.txt file in output dir if it exists
  QString const mapcssDir = QFileInfo(mapcssFile).absoluteDir().absolutePath();
  QString const cityRankFileSrc = mapcssDir + "/city_rank.txt";
  QString const cityRankFileDst = outputDir + "city_rank.txt";
  if (QFile(cityRankFileSrc).size() != 0)
  {
    // try to compile city_rank.txt
    std::ifstream t(to_string(cityRankFileSrc));
    std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
    unique_ptr<drule::ICityRankTable> table = drule::GetCityRankTableFromString(str);
    if (nullptr == table)
      throw runtime_error("File city_rank does not look correctly");

    if (!CopyFile(cityRankFileSrc, cityRankFileDst))
      throw runtime_error("Cannot copy city_rank file");
  }
}

void BuildDrawingRules(QString const & mapcssFile, QString const & outputDir)
{
  QString const resourceDir = GetPlatform().ResourcesDir().c_str();

  if (!QFile::copy(resourceDir + "mapcss-mapping.csv", outputDir + "mapcss-mapping.csv"))
    throw runtime_error("Unable to copy mapcss-mapping.csv file");

  if (!QFile::copy(resourceDir + "mapcss-dynamic.txt", outputDir + "mapcss-dynamic.txt"))
    throw runtime_error("Unable to copy mapcss-dynamic.txt file");

  BuildDrawingRulesImpl(mapcssFile, outputDir);
}

void ApplyDrawingRules(QString const & outputDir)
{
  QString const resourceDir = GetPlatform().ResourcesDir().c_str();

  if (!CopyFile(outputDir + "drules_proto.bin", resourceDir + "drules_proto.bin"))
    throw runtime_error("Cannot copy drawing rules file");
  if (!CopyFile(outputDir + "classificator.txt", resourceDir + "classificator.txt"))
    throw runtime_error("Cannot copy classificator file");
  if (!CopyFile(outputDir + "types.txt", resourceDir + "types.txt"))
    throw runtime_error("Cannot copy types file");

  // Copy city_rank.txt file if such exists or remove from resources if no city_rank.txt file
  if (QFile(outputDir + "city_rank.txt").size() != 0)
  {
    if (!CopyFile(outputDir + "city_rank.txt", resourceDir + "city_rank.txt"))
      throw runtime_error("Cannot copy city_rank file");
  }
  else
  {
    if (QFile::exists(resourceDir + "city_rank.txt") && !QFile::remove(resourceDir + "city_rank.txt"))
      throw runtime_error("Cannot remove city_rank file");
  }
}

}  // namespace build_style
