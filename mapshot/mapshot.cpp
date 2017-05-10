#include "map/framework.hpp"

#include "geometry/mercator.hpp"

#include "base/string_utils.hpp"

#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

#include "3party/gflags/src/gflags/gflags.h"

#pragma mark Define options
//----------------------------------------------------------------------------------------
DEFINE_bool(c, false, "Read places from stdin");
DEFINE_string(place, "", "Define place in format \"lat;lon;zoom\"");
DEFINE_string(outpath, "./", "Path for output files");
DEFINE_string(datapath, "", "Path to data directory");
DEFINE_string(mwmpath, "", "Path to mwm files");
DEFINE_int32(width, 480, "Resulting image width");
DEFINE_int32(height, 640, "Resulting image height");
//----------------------------------------------------------------------------------------

namespace
{
struct Place
{
  double lat;
  double lon;
  int zoom;
  int width;
  int height;
};

Place ParsePlace(std::string const & src)
{
  Place p;
  try
  {
    strings::SimpleTokenizer token(src, ";");
    p.lat = std::stod(*token);
    p.lon = std::stod(*(++token));
    p.zoom = static_cast<int>(std::stoi(*(++token)));
  }
  catch (std::exception & e)
  {
    std::cerr << "Error in [" << src << "]: " << e.what() << endl;
    exit(1);
  }
  return p;
}

void RenderPlace(Framework & framework, Place const & place, std::string const & filename)
{
  df::watch::FrameImage frame;
  df::watch::FrameSymbols sym;
  sym.m_showSearchResult = false;

  // If you are interested why, look at CPUDrawer::CalculateScreen.
  // It is almost UpperComfortScale but there is some magic involved.
  int constexpr kMagicBaseScale = 17;

  framework.DrawWatchFrame(MercatorBounds::FromLatLon(place.lat, place.lon), place.zoom - kMagicBaseScale,
                           place.width, place.height, sym, frame);

  std::ofstream file(filename.c_str());
  file.write(reinterpret_cast<char const *>(frame.m_data.data()), frame.m_data.size());
  file.close();
}

std::string FilenameSeq(std::string const & path)
{
  static size_t counter = 0;
  stringstream filename;
  filename << path << "mapshot" << std::setw(6) << std::setfill('0') << counter++ << ".png";
  return filename.str();
}
}  // namespace

int main(int argc, char * argv[])
{
  google::SetUsageMessage(
      "Generate screenshots of MAPS.ME maps in chosen places, specified by coordinates and zoom.");
  google::ParseCommandLineFlags(&argc, &argv, true);

  if (!FLAGS_c && FLAGS_place.empty())
  {
    std::cerr << "Either -c or -place must be set" << endl;
    return 1;
  }

  if (!FLAGS_datapath.empty())
    GetPlatform().SetResourceDir(FLAGS_datapath);

  if (!FLAGS_mwmpath.empty())
    GetPlatform().SetWritableDirForTests(FLAGS_mwmpath);

  try
  {
    Framework f;

    auto processPlace = [&](std::string const & place)
    {
      Place p = ParsePlace(place);
      p.width = FLAGS_width;
      p.height = FLAGS_height;
      std::string const & filename = FilenameSeq(FLAGS_outpath);
      RenderPlace(f, p, filename);
      std::cout << "Rendering " << place << " into " << filename << " is finished." << endl;
    };

    // This magic constant was determined in several attempts.
    // It is a scale level, basically, dpi factor. 1 means 90 or 96, it seems,
    // and with 1.1 the map looks subjectively better.
    f.InitWatchFrameRenderer(1.1 /* visualScale */);

    if (!FLAGS_place.empty())
      processPlace(FLAGS_place);

    if (FLAGS_c)
    {
      for (std::string line; std::getline(std::cin, line);)
        processPlace(line);
    }

    f.ReleaseWatchFrameRenderer();
    return 0;
  }
  catch (std::exception & e)
  {
    std::cerr << e.what() << endl;
  }
  return 1;
}
