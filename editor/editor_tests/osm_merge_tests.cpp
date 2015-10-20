#include "testing/testing.hpp"

#include "std/string.hpp"

string const osmChangeXml =
"<osmChange version=\"0.6\" generator=\"MAPS.ME\">" \
"  <modify>" \
"    <node id=\"1234\" changeset=\"42\" version=\"2\" lat=\"12.1234567\" lon=\"-8.7654321\"/>" \
"      <tag k=\"amenity\" v=\"school\"/>" \
"    </node>" \
"  </modify>" \
"</osmChange>";


UNIT_TEST(OSM_Merge_Smoke)
{
  // TODO(AlexZ)
//  string const empty;
//  TEST_EQUAL(MergeEditsWithOSM(osmChangeXml, empty), osmChangeXml, ());
}
