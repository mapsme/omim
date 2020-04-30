#include "indexer/features_tag.hpp"

#include "base/macros.hpp"

#include "defines.hpp"

using namespace std;

string GetFeaturesTag(FeaturesTag tag, version::Format format)
{
  if (tag == FeaturesTag::Isolines)
    return ISOLINES_FEATURES_FILE_TAG;

  CHECK_EQUAL(tag, FeaturesTag::Common, ());
  if (format < version::Format::v10)
    return FEATURES_FILE_TAG_V1_V9;
  return FEATURES_FILE_TAG;
}

string GetFeaturesOffsetsTag(FeaturesTag tag)
{
  if (tag == FeaturesTag::Common)
    return FEATURE_OFFSETS_FILE_TAG;
  CHECK_EQUAL(tag, FeaturesTag::Isolines, ());
  return ISOLINES_OFFSETS_FILE_TAG;
}

string GetIndexTag(FeaturesTag tag)
{
  if (tag == FeaturesTag::Common)
    return INDEX_FILE_TAG;
  CHECK_EQUAL(tag, FeaturesTag::Isolines, ());
  return ISOLINES_INDEX_FILE_TAG;
}

string GetGeometryFileTag(FeaturesTag tag)
{
  if (tag == FeaturesTag::Common)
    return GEOMETRY_FILE_TAG;
  CHECK_EQUAL(tag, FeaturesTag::Isolines, ());
  return ISOLINES_GEOMETRY_FILE_TAG;
}
string GetTrianglesFileTag(FeaturesTag tag)
{
  if (tag == FeaturesTag::Common)
    return TRIANGLE_FILE_TAG;
  CHECK_EQUAL(tag, FeaturesTag::Isolines, ());
  return ISOLINES_TRIANGLE_FILE_TAG;
}

string DebugPrint(FeaturesTag tag)
{
  switch (tag)
  {
  case FeaturesTag::Common: return "Common";
  case FeaturesTag::Isolines: return "Isolines";
  case FeaturesTag::Count: return "Count";
  }
  UNREACHABLE();
}
