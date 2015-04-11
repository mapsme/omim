#pragma once

#include "../std/string.hpp"
#include "../std/tuple.hpp"
#include "../std/map.hpp"
#include "../std/exception.hpp"
#include "../std/chrono.hpp"
#include "osm_data.hpp"

using namespace osm;

namespace edit
{
  enum EError
  {
    OK = 0,
    STORAGE_ERROR = 1
  };

  class Exception : public runtime_error
  {
    public:
    size_t errCode = 0;
    Exception(size_t _errCode, std::string const &_errMsg) : runtime_error(_errMsg), errCode(_errCode) {}
  };

  struct DataValue
  {
    string old_value;
    string new_value;

    DataValue() = default;
    DataValue(string const &ov, string const & nv) : old_value(ov), new_value(nv) {}
  };

  enum EDataKey
  {
    LAT = 100, LON = 101, TYPE = 102,
    NAME = 1,
    OPENING_HOURS = 2,
    HOUSENUMBER = 3,
    HOUSENAME = 4,
    STREET = 5,
    POSTCODE = 6,
    PHONE = 7,
    WEBSITE = 8,
    EMAIL = 9,
    OPERATOR = 10,
    CUISINE = 11,
    STARS = 12,
    ELE = 13
  };

  struct MWMLink
  {
    double lon;
    double lat;
    uint32_t type;
    uint32_t key;

    MWMLink(double _lon, double _lat, uint32_t _type) : lon(_lon), lat(_lat), type(_type), key(0) {}
  };

//  using TMWMLink = tuple<double /* lon */, double /* lat */, uint32_t /*type*/>;
  using TChanges = map<EDataKey, DataValue>;

  class FeatureDiff
  {
  public:
    enum EState
    {
        CREATED, MODIFIED, DELETED
    };

    MWMLink id;
    time_t created;
    time_t modified;
    time_t uploaded = 0;
    EState state = CREATED;

    uint64_t version; // internal

    TChanges changes;

    FeatureDiff() : id(0,0,0) {}

    FeatureDiff(MWMLink const & _id) : id(_id), version(0)
    {
      created = modified = system_clock::to_time_t(system_clock::now());
    }

    FeatureDiff(FeatureDiff const &base, TChanges const & val) : FeatureDiff(base.id)
    {
      version = base.version + 1;
      SetState(edit::FeatureDiff::MODIFIED);
      changes = base.changes;
      for (auto const & e : val)
        changes[e.first] = e.second;
      created = base.created;
    }

    FeatureDiff & AddChange(EDataKey key, string const & old_value, string const & new_value)
    {
      changes.emplace(key, DataValue(old_value, new_value));
      modified = system_clock::to_time_t(system_clock::now());
      return *this;
    }

    FeatureDiff & SetState(EState s)
    {
      state = s;
      modified = system_clock::to_time_t(system_clock::now());
      return *this;
    }

  };

  class FeatureChangeset
  {

  public:
    FeatureChangeset();
    void CreateChange(MWMLink const & id, TChanges const &values);
    void ModifyChange(MWMLink const & id, TChanges const &values);
    void DeleteChange(MWMLink const & id);
    bool Find(MWMLink const & id, FeatureDiff * diff = nullptr);


    void UploadChangeset(string const & user, string const & password);
  };
}
