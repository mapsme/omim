#include "../osm_data.hpp"
#include "../../3party/Alohalytics/src/http_client.h"
#include "../../coding/parse_xml.hpp"
#include "../../std/sstream.hpp"
#include "../../testing/testing.hpp"

using namespace osm;

extern string const OSM_SERVER_URL;

class StringSequence
{
  istringstream iss;

public:
  StringSequence(string const & str) : iss(istringstream(str)) {}
  uint64_t Read(char * p, uint64_t size) { iss.read(p, size); return iss.gcount(); }
};

UNIT_TEST(OsmData)
{
  m2::PointD p1(10, 10);
  m2::PointD p2(20, 10);
  m2::PointD p3(5, 5);
  OsmNode nd1(1, p1);
  OsmNode nd2(2, p2);
  OsmWay w1(1);
  w1.Add(&nd1);
  w1.Add(&nd2);
  w1.SetValue("highway", "primary");
  OsmNode nd3(3, p3);
  nd3.SetValue("amenity", "bench");
  OsmWay w2(2);
  w2.Add(&nd2);
  w2.Add(&nd3);
  w2.Add(&nd1);
  w2.Add(&nd2);
  OsmNode nd4(4);
  OsmWay w3(3);
  w3.Add(&nd2);
  w3.Add(&nd4);
  OsmRelation r1(1);
  r1.Add(&w3);
  r1.Add(&w2, "other");

  // test accessors
  TEST(!w1.IsIncomplete(), ());
  TEST(w3.IsIncomplete(), ());
  TEST(r1.IsIncomplete(), ());
  TEST(!w1.IsClosed(), ());
  TEST(w2.IsClosed(), ());
  TEST(nd3.GetValue("amenity") == "bench", ());
  TEST(w1.GetCenter() == m2::PointD(15, 10), ());

  // now data
  OsmData data;
  data.Add(&nd1);
  data.Add(&nd2);
  data.Add(&nd3);
  data.Add(&nd4);
  data.Add(&w1);
  data.Add(&w2);
  data.Add(&w3);
  data.Add(&r1);
  data.BuildAreas();
  TEST(!w1.IsArea(), ());
  TEST(data.GetWay(2)->IsArea(), ());

  // to xml...
  ostringstream oss;
  data.XML(oss);
  string xml(oss.str());
  TEST(!xml.empty(), ());
  cout << xml;

  // ...and back again
  OsmData parsed;
  OsmParser parser(parsed);
  StringSequence source(xml);
  ParseXMLSequence(source, parser);
  TEST(!parsed.Empty(), ());
  TEST(parsed.HasNode(3), ());
  TEST(parsed.HasWay(2), ());
  TEST(parsed.HasRelation(1), ());
  TEST(!parsed.GetWay(1)->IsIncomplete(), ());
  TEST(parsed.GetNode(3)->GetValue("amenity") == "bench", ());
}

UNIT_TEST(OsmChange)
{
  m2::PointD p1(10, 10);
  OsmNode nd(0, p1);
  nd.SetValue("addr:street", "улица Ильича");
  nd.SetValue("addr:housenumber", "6");

  OsmChange osc;
  osc.Comment("Created an address");
  osc.Create(&nd);

  TEST(!osc.Empty(), ());

  ostringstream oss;
  osc.ChangesetXML(oss);
  string xml(oss.str());
  TEST(!xml.empty(), ());
  cout << xml;

  oss.str("");
  osc.XML(oss, 12345);
  xml = oss.str();
  TEST(!xml.empty(), ());
  cout << xml;
}

UNIT_TEST(OsmUpload)
{
  return;

  m2::PointD p1(10, 10);
  OsmNode nd(4298639709, p1);
  nd.SetValue("addr:street", "улица Ильича");
  nd.SetValue("addr:housenumber", "7");

  OsmChange osc;
  osc.Comment("Created an address");
  osc.Modify(&nd);

  string const user = "ZverikI";
  string const password = "qwerty12";
  string const baseUrl = OSM_SERVER_URL + "/api/0.6/changeset/";
  alohalytics::HTTPClientPlatformWrapper create(baseUrl + "create");
  create.set_user_and_password(user, password);
  ostringstream oss;
  osc.ChangesetXML(oss);
  cout << oss.str();
  create.set_body_data(oss.str(), "text/xml", "PUT")
      .set_debug_mode(true);
  TEST(create.RunHTTPRequest() && 200 == create.error_code(), ("Failed to create changeset", create.error_code()));

  string const changesetID = create.server_response();
  OsmId changeset_id;
  strings::to_int64(changesetID, changeset_id);
  TEST(changeset_id > 0, (changeset_id));

  create.set_url_requested(baseUrl + changesetID + "/upload");
  oss.str("");
  osc.XML(oss, changeset_id);
  cout << oss.str();
  create.set_body_data(oss.str(), "text/xml", "POST");
  bool result = create.RunHTTPRequest() && 200 == create.error_code();
  TEST(result, ("Failed to upload data", create.error_code()));

  create.set_url_requested(baseUrl + changesetID + "/close");
  create.set_body_data("", "text/xml", "PUT");
  create.RunHTTPRequest();
  TEST(create.error_code() == 200, ("Failed to close the changeset", create.error_code()));
}
