#include "featurechangeset.hpp"

#include "sqlite3.h"

#include "../std/iostream.hpp"
#include "../std/sstream.hpp"
#include "../std/iomanip.hpp"
#include "../std/limits.hpp"
#include "../std/vector.hpp"
#include "../std/algorithm.hpp"


namespace
{
  static const double kModifier = 1E7;

  string to_string(edit::MWMLink const & l)
  {
    stringstream s;
    s << hex << uppercase << setfill('0') << setw(8) << int32_t(l.lon * kModifier);
    s << hex << uppercase << setfill('0') << setw(8) << int32_t(l.lat * kModifier);
    s << hex << uppercase << setfill('0') << setw(8) << int32_t(l.type);
    s << hex << uppercase << setfill('0') << setw(8) << int32_t(l.key);
    return s.str();
  }

  edit::MWMLink from_string(string const & s)
  {
    edit::MWMLink link(double(int32_t(stol(s.substr(0,8),0,16))) / kModifier
                       ,double(int32_t(stol(s.substr(8,8),0,16))) / kModifier
                       ,stoi(s.substr(16,8),0,16));
    link.key = stoi(s.substr(24,8),0,16);
    return link;
  }

  ostream & operator << (ostream & s , edit::FeatureDiff const & e)
  {
    s << "ID: " << to_string(e.id) << " " << e.created << " " << e.modified << " " << e.uploaded << " version(" << e.version << ")" << endl;
    s << "\tState: " << e.state << endl;
    for (auto const & c : e.changes) {
      s << "\t DataKey: " << c.first << " old_value: " << c.second.old_value << " new_value: " << c.second.new_value << endl;
    }
    return s;
  }

  void InitStorage()
  {
    sqlite3 *db;
    // Open database
    if (sqlite3_open("changes.db", &db))
      throw edit::Exception(edit::STORAGE_ERROR, sqlite3_errmsg(db));

    // Create SQL statement
    char const *sql = "CREATE TABLE IF NOT EXISTS actions ("  \
    "id           TEXT    NOT NULL," \
    "created      INT     NOT NULL," \
    "modified     INT," \
    "uploaded     INT DEFAULT NULL," \
    "version      INT     NOT NULL," \
    "state        INT," \
    "changes      BLOB );";

    // Execute SQL statement
    if (sqlite3_exec(db, sql, NULL, 0, NULL) != SQLITE_OK)
      throw edit::Exception(edit::STORAGE_ERROR, sqlite3_errmsg(db));
    sqlite3_close(db);
  }


  string serialize(edit::TChanges const &changes)
  {
    stringstream ss;
    for (auto const & e: changes)
    {
      uint8_t last_key_mark = (&e == &(*changes.crbegin())) << 7; /// set high bit (0x80) if it last element
      uint8_t elem[3] = {static_cast<uint8_t>(e.first | last_key_mark)
        ,static_cast<uint8_t>(min(e.second.old_value.size(), (size_t)numeric_limits<uint8_t>::max()))
        ,static_cast<uint8_t>(min(e.second.new_value.size(), (size_t)numeric_limits<uint8_t>::max()))
      };
      ss.write((char const *)elem, sizeof(elem));
      ss.write(e.second.old_value.data(), elem[1]);
      ss.write(e.second.new_value.data(), elem[2]);
    }
    return ss.str();
  }

  void SaveToStorage(edit::FeatureDiff const &diff)
  {
    sqlite3 *db;
    // Open database
    if (sqlite3_open("changes.db", &db))
      throw edit::Exception(edit::STORAGE_ERROR, sqlite3_errmsg(db));

    // Create SQL statement
    stringstream ss;
    ss << "INSERT INTO actions VALUES(";
    ss << '\'' << to_string(diff.id) << '\'';
    ss << ',' << diff.created << ',' << diff.modified << ',' << (diff.uploaded ? std::to_string(diff.uploaded) : "NULL");
    ss << ',' << diff.version;
    ss << ',' << diff.state;
    ss << ",X'";
    for (char c : serialize(diff.changes))
      ss << hex << uppercase << setfill('0') << setw(2) << uint16_t(c & 0xFF);
    ss << "');";

    // Execute SQL statement
    if (sqlite3_exec(db, ss.str().c_str(), NULL, 0, NULL) != SQLITE_OK)
      throw edit::Exception(edit::STORAGE_ERROR, sqlite3_errmsg(db));
    sqlite3_close(db);
  }


  void LoadFromStorage(edit::MWMLink const & id, vector<edit::FeatureDiff> & diffs)
  {

    sqlite3 *db;
    // Open database
    if (sqlite3_open("changes.db", &db))
      throw edit::Exception(edit::STORAGE_ERROR, sqlite3_errmsg(db));

    // Create SQL statement
    string sql("SELECT * FROM actions WHERE id=? ORDER BY version ASC");

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql.c_str(), static_cast<int>(sql.size()), &stmt, NULL);

    string const & ssid = to_string(id);
    sqlite3_bind_text(stmt, 1, ssid.c_str(), static_cast<int>(ssid.size()), SQLITE_STATIC);

    // Execute SQL statement
    int rc = 0;
    while ( rc != SQLITE_DONE)
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

} // anonymouse namespace

namespace edit
{

  FeatureChangeset::FeatureChangeset()
  {
    InitStorage();
  }

  void FeatureChangeset::CreateChange(MWMLink const & id, TChanges const &values)
  {
    edit::FeatureDiff fd(id);
    fd.SetState(edit::FeatureDiff::CREATED);
    for (auto const & e : values) {
      fd.changes.insert(e);
    }
    SaveToStorage(fd);
  }

  void FeatureChangeset::ModifyChange(MWMLink const & id, TChanges const &values)
  {
    vector<edit::FeatureDiff> diffs;
    LoadFromStorage(id, diffs);
    edit::FeatureDiff nd(diffs.back(), values);
    SaveToStorage(nd);
  }

  void FeatureChangeset::DeleteChange(MWMLink const & id)
  {
    vector<edit::FeatureDiff> diffs;
    LoadFromStorage(id, diffs);
    edit::FeatureDiff nd(diffs.back(), TChanges());
    nd.SetState(edit::FeatureDiff::DELETED);
    SaveToStorage(nd);
  }

  bool FeatureChangeset::Find(MWMLink const & id, FeatureDiff * diff)
  {
    vector<edit::FeatureDiff> diffs;
    LoadFromStorage(id, diffs);
    if (diff)
      *diff = diffs.back();
    return !diffs.empty();
  }

} // namespace edit
