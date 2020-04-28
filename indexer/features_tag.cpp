#include "indexer/features_tag.hpp"

#include "base/macros.hpp"

#include "defines.hpp"

using namespace std;

string GetFeaturesTag(FeaturesTag tag, version::Format format)
{
  CHECK_EQUAL(tag, FeaturesTag::Common, ());
  if (format < version::Format::v10)
    return FEATURES_FILE_TAG_V1_V9;
  return FEATURES_FILE_TAG;
}

string GetFeaturesOffsetsTag(FeaturesTag tag)
{
  CHECK_EQUAL(tag, FeaturesTag::Common, ());
  return FEATURE_OFFSETS_FILE_TAG;
}

string GetIndexTag(FeaturesTag tag)
{
  CHECK_EQUAL(tag, FeaturesTag::Common, ());
  return INDEX_FILE_TAG;
}

string GetGeometryFileTag(FeaturesTag tag)
{
  CHECK_EQUAL(tag, FeaturesTag::Common, ());
  return GEOMETRY_FILE_TAG;
}

string GetTrianglesFileTag(FeaturesTag tag)
{
  CHECK_EQUAL(tag, FeaturesTag::Common, ());
  return TRIANGLE_FILE_TAG;
}

string DebugPrint(FeaturesTag tag)
{
  switch (tag)
  {
  case FeaturesTag::Common: return "Common";
  case FeaturesTag::Count: return "Count";
  }
  UNREACHABLE();
}
