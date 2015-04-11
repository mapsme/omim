#include "featurechangeset.hpp"

#include "../base/string_utils.hpp"
#include "../base/assert.hpp"
#include "../coding/parse_xml.hpp"
#include "../geometry/point2d.hpp"
#include "../geometry/rect2d.hpp"
#include "../indexer/classificator.hpp"
#include "../platform/http_request.hpp"
#include "../platform/platform.hpp"
#include "../std/string.hpp"
#include "../std/sstream.hpp"
#include "sqlite3.h"
#include "../3party/Alohalytics/src/http_client.h"

namespace
{
  static const double kModifier = 1E7;

  edit::MWMLink from_string(string const & s)
  {
    edit::MWMLink link(double(int32_t(stol(s.substr(0,8),0,16))) / kModifier
                       ,double(int32_t(stol(s.substr(8,8),0,16))) / kModifier
                       ,stoi(s.substr(16,8),0,16));
    link.key = stoi(s.substr(24,8),0,16);
    return link;
  }

  void LoadAllFromStorage(vector<edit::FeatureDiff> & diffs)
  {
    sqlite3 *db;
    // Open database
    if (sqlite3_open("changes.db", &db))
      throw edit::Exception(edit::STORAGE_ERROR, sqlite3_errmsg(db));

    // Create SQL statement
    string sql("SELECT * FROM actions ORDER BY version ASC");

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql.c_str(), static_cast<int>(sql.size()), &stmt, NULL);

    // Execute SQL statement
    int rc = 0;
    while (rc != SQLITE_DONE)
    {
      switch (rc = sqlite3_step(stmt))
      {
        case SQLITE_ROW:
        {
          string s((char const *)sqlite3_column_text(stmt, 0), sqlite3_column_bytes(stmt, 0));
          diffs.emplace_back(from_string(s));
          edit::FeatureDiff & diff = diffs.back();

          diff.created = sqlite3_column_int(stmt, 1);
          diff.modified = sqlite3_column_int(stmt, 2);
          diff.uploaded = (sqlite3_column_type(stmt, 3) == SQLITE_NULL) ? 0 : sqlite3_column_int(stmt, 3);
          diff.version = sqlite3_column_int(stmt, 4);
          diff.state = (edit::FeatureDiff::EState)sqlite3_column_int(stmt, 5);

          istringstream ss(string((char const *)sqlite3_column_text(stmt, 6), sqlite3_column_bytes(stmt, 6)));

          uint8_t header[3] = {0};
          char buffer1[uint8_t(-1)] = {0};
          char buffer2[uint8_t(-1)] = {0};
          do
          {
            ss.read((char *)header, sizeof(header));
            ss.read(buffer1, header[1]);
            ss.read(buffer2, header[2]);
            diff.changes.emplace(static_cast<edit::EDataKey>(header[0] & 0x7F)
                                 , edit::DataValue(string(buffer1, header[1]), string(buffer2, header[2])));
          } while (!(header[0] & 0x80));
          break;
        }
        case SQLITE_DONE: break;
        default:
          throw edit::Exception(edit::STORAGE_ERROR, sqlite3_errmsg(db));
      }
    }
    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
  }
}

namespace
{
  double const BBOX_RADIUS = 1e-5;

  class StringSequence
  {
    istringstream iss;

  public:
    StringSequence(string const & str) : iss(istringstream(str)) {}
    uint64_t Read(char * p, uint64_t size) { iss.read(p, size); return iss.gcount(); }
  };

  bool StringsToPoint(string const & lon, string const & lat, m2::PointD & pt)
  {
    double d_lon, d_lat;
    if (!strings::to_double(lon, d_lon) || !strings::to_double(lat, d_lat))
      return false;
    pt = m2::PointD(d_lon, d_lat);
    return true;
  }

  bool ExtractTagPair(string source, size_t & start, pair<string, string> & tag)
  {
    size_t const key_start = source.find('[', start);
    if (key_start == string::npos)
      return false;
    size_t const comma = source.find(',', start);
    if (comma != string::npos && comma < key_start)
      return false;
    size_t key_end = source.find('=', key_start);
    if (key_end == string::npos)
      key_end = source.find(']', key_start);
    if (key_end == string::npos || key_end - key_start <= 1)
      return false;
    size_t const value_end = source.find(']', key_end);

    string const key = source.substr(key_start + 1, key_end - key_start - 1);
    string value;
    if (value_end != string::npos && value_end - key_end > 1)
      value = source.substr(key_end + 1, value_end - key_end - 1);
    else
    {
      if (key[0] == '!')
        value = "no";
      else if (key[key.length() - 1] == '?')
        value = "yes";
      else
        value = "";
    }
    tag = pair<string, string>(key, value);
    start = value_end + 1;
    return true;
  }

  bool HasTags(OsmElement * element, vector<pair<string, string>> tags)
  {
    for (pair<string, string> const & tag : tags)
    {
      if (!element->HasKey(tag.first))
        return false;
      if (!tag.second.empty())
      {
        if (tag.second == "yes")
        {
          if (!(element->GetValue(tag.first) == "yes" || element->GetValue(tag.first) == "true" || element->GetValue(tag.first) == "1"))
            return false;
        }
        if (tag.second == "no")
        {
          if (!(element->GetValue(tag.first) == "no" || element->GetValue(tag.first) == "false" || element->GetValue(tag.first) == "0"))
            return false;
        }
        else if (element->GetValue(tag.first) != tag.second)
          return false;
      }
    }
    return true;
  }

  bool TypeToTags(uint32_t type, vector<pair<string, string>> & tags)
  {
    string csv_data;
    {
      ReaderPtr<Reader>(GetPlatform().GetReader("mapcss-mapping.csv")).ReadAsString(csv_data);
    }

    string const type_str = classif().GetFullObjectName(type);
    tags.clear();
    istringstream lines(csv_data);
    string line;
    getline(lines, line);
    while (!lines.fail() && tags.empty())
    {
      istringstream fields(line);
      string mapping_type, mapping_tags;
      getline(fields, mapping_type, ';');
      getline(fields, mapping_tags, ';');
      if (!fields.fail() && mapping_type == type_str)
      {
        // found type, extract tags
        // amenity|parking;[amenity=parking][access?], [amenity=parking][!access];;name;int_name;26;
        pair<string, string> tag;
        size_t start = 0;
        while (ExtractTagPair(mapping_tags, start, tag))
          tags.push_back(pair<string, string>(tag));
        break;
      }
      getline(lines, line);
    }
    return !tags.empty();
  }

  bool FindClosestMatch(m2::PointD const & center, vector<pair<string, string>> const & tags,
                          vector<OsmElement *> const & data, OsmElement * match)
  {
    double distance = 1.0L;
    for (OsmElement * element : data)
    {
      double const sq_l = element->GetCenter().SquareLength(center);
      if (sq_l < distance && HasTags(element, tags))
      {
        distance = sq_l;
        *match = *element;
      }
    }
    return distance < 1.0L;
  }

  string GetBBoxQueryURL(m2::PointD const & center, double radius = BBOX_RADIUS)
  {
    m2::RectD const rect = m2::Inflate(m2::RectD(center, center), radius, radius);
    ostringstream url;
    m2::PointD const lb = rect.LeftBottom();
    m2::PointD const rt = rect.RightTop();
    url << "map?bbox=" << lb.x << ',' << lb.y << ',' << rt.x << rt.y;
    return url.str();
  }
}

namespace edit
{
  // TODO: change to api.openstreetmap.org eventually
  string const OSM_SERVER = "http://api06.dev.openstreetmap.org/api/0.6/";
  // TODO: proper authorization
  string const OSM_USER = "ZverikI";
  string const OSM_PASSWORD = "qwerty12";

  void LinkToChanges(MWMLink const & link, TChanges & changes)
  {
    if (!changes.count(EDataKey::LON))
      changes[EDataKey::LON] = { strings::to_string(link.lon), "" };
    if (!changes.count(EDataKey::LAT))
      changes[EDataKey::LAT] = { strings::to_string(link.lat), "" };
    if (!changes.count(EDataKey::TYPE))
      changes[EDataKey::TYPE] = { strings::to_string(link.type), "" };
  }

  void AddNewTags(OsmElement *el, TChanges const & changes)
  {
    if (changes.count(EDataKey::HOUSENUMBER))
      el->SetValue("addr:housenumber", changes.at(EDataKey::HOUSENUMBER).new_value);
    if (changes.count(EDataKey::STREET))
      el->SetValue("addr:street", changes.at(EDataKey::STREET).new_value);
    // TODO: fill other tags
  }

  void FindChanges(FeatureDiff const & diff, OsmChange & osc)
  {
    TChanges changes(diff.changes);
    LinkToChanges(diff.id, changes);
    vector<pair<string, string>> typeTags;
    uint32_t old_type;
    CHECK(strings::to_uint32(changes[EDataKey::TYPE].old_value, old_type), ());
    if (!TypeToTags(old_type, typeTags))
      return; // TODO: cannot process this object. Notify?
    m2::PointD center;
    CHECK(StringsToPoint(changes[EDataKey::LON].old_value, changes[EDataKey::LAT].old_value, center), ());
    if (diff.state == FeatureDiff::EState::CREATED)
    {
      OsmNode created(center);
      for (pair<string, string> const & tag : typeTags)
        if (!tag.second.empty())
          created.SetValue(tag.first, tag.second);
      AddNewTags(&created, changes);
      osc.Create(&created);
    }
    else
    {
      // modification or deletion: we need an original element.
      // now we have tags, let's query bbox at point and find an object that matches tags
      alohalytics::HTTPClientPlatformWrapper create(OSM_SERVER + GetBBoxQueryURL(center));
      if (create.RunHTTPRequest() && 200 == create.error_code())
      {
        string const xml = create.server_response();
        OsmData data;
        OsmParser parser(data);
        StringSequence source(xml);
        ParseXMLSequence(source, parser);
        // downloaded data, search for nearest point
        data.BuildAreas();
        OsmElement * closest = nullptr;
        if (!FindClosestMatch(center, typeTags, data.GetElements(OsmType::NODE), closest) &&
            !FindClosestMatch(center, typeTags, data.GetElements(OsmType::AREA), closest))
        {
          // did not find relevant feature?
          return;
        }
        if (diff.state == FeatureDiff::EState::DELETED)
        {
          osc.Delete(closest);
        }
        else
        {
          OsmElement *modified(closest);
          // TODO: we have an element, update its tags
          osc.Modify(modified);
        }
      };
    }
  }

  bool UploadChanges(OsmChange const & osc, OsmData & uploaded)
  {
    string const baseUrl = OSM_SERVER + "changeset/";
    alohalytics::HTTPClientPlatformWrapper create(baseUrl + "create");
    // TODO: proper authorization
    create.set_user_and_password(OSM_USER, OSM_PASSWORD);
    ostringstream oss;
    osc.ChangesetXML(oss);
    create.set_body_data(oss.str(), "text/xml", "PUT")
        .set_debug_mode(true);
    if (!create.RunHTTPRequest() || 200 != create.error_code())
      return false;
    string const changesetID = create.server_response();
    OsmId changeset_id;
    strings::to_int64(changesetID, changeset_id);

    create.set_url_requested(baseUrl + changesetID + "/upload");
    oss.str("");
    osc.XML(oss, changeset_id);
    create.set_body_data(oss.str(), "text/xml", "POST");
    bool result = create.RunHTTPRequest() && 200 == create.error_code();
    // TODO: move correct data to "uploaded" and inform about failed?
    // TODO: proper error handling

    create.set_url_requested(baseUrl + changesetID + "/close");
    create.set_body_data("", "text/xml", "PUT");
    create.RunHTTPRequest();
    return result;
  }

  void FeatureChangeset::UploadChangeset()
  {
    OsmChange osc;
    vector<FeatureDiff> changeset;
    LoadAllFromStorage(changeset);
    for (FeatureDiff & diff : changeset)
    {
      // TODO: error handling
      FindChanges(diff, osc);
    }
    if (!osc.Empty())
    {
      OsmData uploaded;
      UploadChanges(osc, uploaded);
      // TODO: create modified features from "uploaded" and send them somewhere
    }
  }
}
