#include "generator/generator_tests/common.hpp"

#include "generator/borders.hpp"

#include "platform/platform.hpp"

#include "base/file_name_utils.hpp"
#include "base/string_utils.hpp"

#include <fstream>

#include "defines.hpp"

namespace generator_tests
{
OsmElement MakeOsmElement(uint64_t id, Tags const & tags, OsmElement::EntityType t)
{
  OsmElement el;
  el.m_id = id;
  el.m_type = t;
  for (auto const & t : tags)
    el.AddTag(t.first, t.second);

  return el;
}

std::string GetFileName()
{
  auto & platform = GetPlatform();
  auto const tmpDir = platform.TmpDir();
  platform.SetWritableDirForTests(tmpDir);
  return platform.TmpPathForFile();
}

bool MakeFakeBordersFile(std::string const & intemediatePath, std::string const & filename)
{
  auto const borderPath = base::JoinPath(intemediatePath, BORDERS_DIR);
  auto & platform = GetPlatform();
  auto const code = platform.MkDir(borderPath);
  if (code != Platform::EError::ERR_OK && code != Platform::EError::ERR_FILE_ALREADY_EXISTS)
    return false;

  std::ofstream file;
  file.exceptions(std::ios::failbit | std::ios::badbit);
  file.open(base::JoinPath(borderPath, filename + ".poly"));
  file << filename << "\n1\n\t-180.0	-90.0\n\t180.0	-90.0\n\t180.0	90.0\n\t-180.0	90.0\n\t-180.0	-90.0\nEND\nEND";
  return true;
}
}  // namespace generator_tests
