#include "generator.hpp"

#include "base/logging.hpp"
#include <iostream>

#include <QApplication>
#include <QtCore/QFile>
#include <QtCore/QString>
#include <QtCore/QHash>

#include <QtXml/QXmlSimpleReader>
#include <QtXml/QXmlInputSource>

#include "3party/gflags/src/gflags/gflags.h"

DEFINE_string(fontFileName, "../../data/01_dejavusans.ttf", "path to TrueType font file");
DEFINE_string(symbolsFile, "../../data/results.unicode", "file with 2bytes symbols for which the skin should be generated");
DEFINE_string(symbolsDir, "../../data/styles/symbols", "directory with svg symbol files");
DEFINE_int32(symbolWidth, 24, "width of the rendered symbol");
DEFINE_int32(symbolHeight, 24, "height of the rendered symbol");
DEFINE_string(skinName, "../../data/basic", "prefix for the skin and skinImage file name");
DEFINE_string(skinSuffix, "mdpi", "suffix for skinName<suffix>.skn and symbols<suffix>.png");
DEFINE_string(searchIconsOutPath, "../../data/search-icons/png", "output path for search category icons");
DEFINE_string(searchCategories, "../../data/search-icons/categories-icons.txt", "path to file that contains mapping between category and icon names");
DEFINE_string(searchIconsSrcPath, "../../data/search-icons/svg", "input path for search category icons");
DEFINE_int32(searchIconWidth, 24, "width of the search category icon");
DEFINE_int32(searchIconHeight, 24, "height of the search category icon");
DEFINE_bool(colorCorrection, false, "apply color correction");
DEFINE_int32(maxSize, 2048, "max width/height of output textures");

int main(int argc, char *argv[])
{
// Used to lock the hash seed, so the order of XML attributes is always the same.
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
  qSetGlobalQHashSeed(0);
#else
  qputenv("QT_HASH_SEED", "0");
#endif
  try
  {
    google::ParseCommandLineFlags(&argc, &argv, true);
    QApplication app(argc, argv);

    tools::SkinGenerator gen(FLAGS_colorCorrection);

    std::vector<QSize> symbolSizes;
    symbolSizes.emplace_back(QSize(FLAGS_symbolWidth, FLAGS_symbolHeight));

    std::vector<std::string> suffixes;
    suffixes.push_back(FLAGS_skinSuffix);

    gen.ProcessSymbols(FLAGS_symbolsDir, FLAGS_skinName, symbolSizes, suffixes);

    if (!gen.RenderPages(FLAGS_maxSize))
    {
      LOG(LINFO, ("Error: The texture is overflown."));
      return 1;
    }

    QString newSkin(FLAGS_skinName.c_str());
    newSkin.replace("basic", "symbols");
    auto const filename = newSkin.toStdString() + FLAGS_skinSuffix + ".sdf";
    if (!gen.WriteToFileNewStyle(filename))
    {
      std::cerr << "Could not write file" << filename << std::endl;
      return -1;
    }

    std::cout << "Done" << std::endl;
    return 0;
  }
  catch (std::exception const & e)
  {
    std::cerr << "Exception " << e.what() << std::endl;
    return -1;
  }
}
