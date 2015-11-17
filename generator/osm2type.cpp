#include "generator/osm2type.hpp"
#include "generator/osm2meta.hpp"
#include "generator/osm_element.hpp"

#include "indexer/classificator.hpp"
#include "indexer/feature_visibility.hpp"
#include "indexer/mercator.hpp"

#include "base/assert.hpp"
#include "base/string_utils.hpp"
#include "base/math.hpp"

#include "std/vector.hpp"
#include "std/bind.hpp"
#include "std/function.hpp"
#include "std/initializer_list.hpp"

#include <QtCore/QString>


namespace ftype
{
  namespace
  {

    bool NeedMatchValue(string const & k, string const & v)
    {
      // Take numbers only for "capital" and "admin_level" now.
      // NOTE! If you add a new type into classificator, which has a number in it
      // (like admin_level=1 or capital=2), please don't forget to insert it here too.
      // Otherwise generated data will not contain your newly added features.
      return !strings::is_number(v) || k == "admin_level" || k == "capital";
    }

    bool IgnoreTag(string const & k, string const & v)
    {
      static string const negativeValues[] = { "no", "false", "-1" };
      // If second component of these pairs is true we need to process this key else ignore it
      static pair<string const, bool const> const processedKeys[] = {
          {"description", true},
          // [highway=primary][cycleway=lane] parsed as [highway=cycleway]
          {"cycleway", true},
          // [highway=proposed][proposed=primary] parsed as [highway=primary]
          {"proposed", true},
          // [highway=primary][construction=primary] parsed as [highway=construction]
          {"construction", true},
          // process in any case
          {"layer", false},
          // process in any case
          {"oneway", false}};

      // Ignore empty key.
      if (k.empty())
        return true;

      // Process special keys.
      for (auto const & key : processedKeys)
      {
        if (k == key.first)
          return key.second;
      }

      // Ignore keys with negative values.
      for (auto const & value : negativeValues)
      {
        if (v == value)
          return true;
      }

      return false;
    }

    template <typename TResult, class ToDo>
    TResult ForEachTag(OsmElement * p, ToDo && toDo)
    {
      TResult res = TResult();
      for (auto & e : p->m_tags)
      {
        if (IgnoreTag(e.key, e.value))
          continue;

        res = toDo(e.key, e.value);
        if (res)
          return res;
      }
      return res;
    }

    template <typename TResult, class ToDo>
    TResult ForEachTagEx(OsmElement * p, set<int> & skipTags, ToDo && toDo)
    {
      int id = 0;
      return ForEachTag<TResult>(p, [&](string const & k, string const & v)
      {
        int currentId = id++;
        if (skipTags.count(currentId) != 0)
          return TResult();
        if (string::npos != k.find("name"))
        {
          skipTags.insert(currentId);
          return TResult();
        }
        TResult res = toDo(k, v);
        if (res)
          skipTags.insert(currentId);
        return res;
      });
    }

    string Normalize(string const & s)
    {
      // Unicode Compatibility Decomposition,
      // followed by Canonical Composition (NFKC).
      // Needed for better search matching.
      QByteArray ba = QString::fromUtf8(s.c_str()).normalized(QString::NormalizationForm_KC).toUtf8();
      return ba.constData();
    }

    class NamesExtractor
    {
      set<string> m_savedNames;
      FeatureParams & m_params;

    public:
      NamesExtractor(FeatureParams & params) : m_params(params) {}

      bool GetLangByKey(string const & k, string & lang)
      {
        strings::SimpleTokenizer token(k, "\t :");
        if (!token)
          return false;

        // Is this an international (latin) name.
        if (*token == "int_name")
        {
          lang = *token;
          return m_savedNames.insert(lang).second;
        }

        if (*token != "name")
          return false;

        ++token;
        lang = (token ? *token : "default");

        // Replace dummy arabian tag with correct tag.
        if (lang == "ar1")
          lang = "ar";

        // Avoid duplicating names.
        return m_savedNames.insert(lang).second;
      }

      bool operator() (string & k, string & v)
      {
        string lang;
        if (v.empty() || !GetLangByKey(k, lang))
          return false;

        m_params.AddName(lang, Normalize(v));
        k.clear();
        v.clear();
        return false;
      }
    };

    class TagProcessor
    {
      template <typename FuncT>
      struct Rule
      {
        char const * key;
        // * - take any values
        // ! - take only negative values
        // ~ - take only positive values
        char const * value;
        function<FuncT> func;
      };

      static bool IsNegative(string const & value)
      {
        for (char const * s : { "no", "none", "false" })
          if (value == s)
            return true;
        return false;
      }

      OsmElement * m_element;

    public:
      TagProcessor(OsmElement * elem) : m_element(elem) {}

      template <typename FuncT = void()>
      void ApplyRules(initializer_list<Rule<FuncT>> const & rules) const
      {
        for (auto & e : m_element->m_tags)
        {
          for (auto const & rule: rules)
          {
            if (strcmp(e.key.data(), rule.key) != 0)
              continue;
            bool take = false;
            if (rule.value[0] == '*')
              take = true;
            else if (rule.value[0] == '!')
              take = IsNegative(e.value);
            else if (rule.value[0] == '~')
              take = !IsNegative(e.value);

            if (take || e.value == rule.value)
              call(rule.func, e.key, e.value);
          }
        }
      }

    protected:
      static void call(function<void()> const & f, string &, string &) { f(); }
      static void call(function<void(string &, string &)> const & f, string & k, string & v) { f(k, v); }
    };
  }

  class CachedTypes
  {
    buffer_vector<uint32_t, 16> m_types;

  public:
    enum EType { ENTRANCE, HIGHWAY, ADDRESS, ONEWAY, PRIVATE, LIT, NOFOOT, YESFOOT,
                 RW_STATION, RW_STATION_SUBWAY };

    CachedTypes()
    {
      Classificator const & c = classif();
      
      for (auto const & e : (StringIL[]) { {"entrance"}, {"highway"} })
        m_types.push_back(c.GetTypeByPath(e));

      StringIL arr[] =
      {
        {"building", "address"}, {"hwtag", "oneway"}, {"hwtag", "private"},
        {"hwtag", "lit"}, {"hwtag", "nofoot"}, {"hwtag", "yesfoot"}
      };
      for (auto const & e : arr)
        m_types.push_back(c.GetTypeByPath(e));

      m_types.push_back(c.GetTypeByPath({ "railway", "station" }));
      m_types.push_back(c.GetTypeByPath({ "railway", "station", "subway" }));
    }

    uint32_t Get(EType t) const { return m_types[t]; }

    bool IsHighway(uint32_t t) const
    {
      ftype::TruncValue(t, 1);
      return t == Get(HIGHWAY);
    }
    bool IsRwStation(uint32_t t) const
    {
      return t == Get(RW_STATION);
    }
    bool IsRwSubway(uint32_t t) const
    {
      ftype::TruncValue(t, 3);
      return t == Get(RW_STATION_SUBWAY);
    }
  };

  void MatchTypes(OsmElement * p, FeatureParams & params)
  {
    set<int> skipRows;
    vector<ClassifObjectPtr> path;
    ClassifObject const * current = nullptr;

    auto matchTagToClassificator = [&path, &current](string const & k, string const & v) -> bool
    {
      // First try to match key.
      ClassifObjectPtr elem = current->BinaryFind(k);
      if (!elem)
        return false;

      path.push_back(elem);

      // Now try to match correspondent value.
      if (!NeedMatchValue(k, v))
        return true;

      if (ClassifObjectPtr velem = elem->BinaryFind(v))
        path.push_back(velem);

      return true;
    };

    do
    {
      current = classif().GetRoot();
      path.clear();

      // Find first root object by key.
      if (!ForEachTagEx<bool>(p, skipRows, matchTagToClassificator))
        break;
      CHECK(!path.empty(), ());

      do
      {
        // Continue find path from last element.
        current = path.back().get();

        // Next objects trying to find by value first.
        ClassifObjectPtr pObj =
            ForEachTagEx<ClassifObjectPtr>(p, skipRows, [&current](string const & k, string const & v)
            {
              if (!NeedMatchValue(k, v))
                return ClassifObjectPtr();
              return current->BinaryFind(v);
            });

        if (pObj)
        {
          path.push_back(pObj);
        }
        else
        {
          // If no - try find object by key (in case of k = "area", v = "yes").
          if (!ForEachTagEx<bool>(p, skipRows, matchTagToClassificator))
            break;
        }
      } while (true);

      // Assign type.
      uint32_t t = ftype::GetEmptyValue();
      for (auto const & e : path)
        ftype::PushValue(t, e.GetIndex());

      // Use features only with drawing rules.
      if (feature::IsDrawableAny(t))
        params.AddType(t);

    } while (true);
  }

  string MatchCity(OsmElement const * p)
  {
    static map<string, m2::RectD> const cities = {
        {"amsterdam", {4.65682983398, 52.232846171, 5.10040283203, 52.4886341706}},
        {"baires", {-58.9910888672, -35.1221551064, -57.8045654297, -34.2685661867}},
        {"barcelona", {1.94458007812, 41.2489025224, 2.29614257812, 41.5414776668}},
        {"beijing", {115.894775391, 39.588757277, 117.026367187, 40.2795256688}},
        {"berlin", {13.0352783203, 52.3051199211, 13.7933349609, 52.6963610783}},
        {"brussel", {4.2448425293, 50.761653413, 4.52499389648, 50.9497757762}},
        {"budapest", {18.7509155273, 47.3034470439, 19.423828125, 47.7023684666}},
        {"chicago", {-88.3163452148, 41.3541338721, -87.1270751953, 42.2691794924}},
        {"delhi", {76.8026733398, 28.3914003758, 77.5511169434, 28.9240352884}},
        {"dnepro", {34.7937011719, 48.339820521, 35.2798461914, 48.6056737841}},
        {"ekb", {60.3588867188, 56.6622647682, 61.0180664062, 57.0287738515}},
        {"frankfurt", {8.36334228516, 49.937079757, 8.92364501953, 50.2296379179}},
        {"hamburg", {9.75860595703, 53.39151869, 10.2584838867, 53.6820686709}},
        {"helsinki", {24.3237304688, 59.9989861206, 25.48828125, 60.44638186}},
        {"kazan", {48.8067626953, 55.6372985742, 49.39453125, 55.9153515154}},
        {"kiev", {30.1354980469, 50.2050332649, 31.025390625, 50.6599083609}},
        {"lisboa", {-9.42626953125, 38.548165423, -8.876953125, 38.9166815364}},
        {"london", {-0.4833984375, 51.3031452592, 0.2197265625, 51.6929902115}},
        {"madrid", {-4.00451660156, 40.1536868578, -3.32885742188, 40.6222917831}},
        {"mexico", {-99.3630981445, 19.2541083164, -98.879699707, 19.5960192403}},
        {"milan", {9.02252197266, 45.341528405, 9.35760498047, 45.5813674681}},
        {"minsk", {27.2845458984, 53.777934972, 27.8393554688, 54.0271334441}},
        {"moscow", {36.9964599609, 55.3962717136, 38.1884765625, 56.1118730004}},
        {"munchen", {11.3433837891, 47.9981928195, 11.7965698242, 48.2530267576}},
        {"newyork", {-74.4104003906, 40.4134960497, -73.4600830078, 41.1869224229}},
        {"nnov", {43.6431884766, 56.1608472541, 44.208984375, 56.4245355509}},
        {"novosibirsk", {82.4578857422, 54.8513152597, 83.2983398438, 55.2540770671}},
        {"osaka", {134.813232422, 34.1981730963, 136.076660156, 35.119908571}},
        {"oslo", {10.3875732422, 59.7812868211, 10.9286499023, 60.0401604652}},
        {"paris", {2.09014892578, 48.6637569323, 2.70538330078, 49.0414689141}},
        {"roma", {12.3348999023, 41.7672146942, 12.6397705078, 42.0105298189}},
        {"sanfran", {-122.72277832, 37.1690715771, -121.651611328, 38.0307856938}},
        {"seoul", {126.540527344, 37.3352243593, 127.23815918, 37.6838203267}},
        {"shanghai", {119.849853516, 30.5291450367, 122.102050781, 32.1523618947}},
        {"spb", {29.70703125, 59.5231755354, 31.3110351562, 60.2725145948}},
        {"stockholm", {17.5726318359, 59.1336814082, 18.3966064453, 59.5565918857}},
        {"sydney", {150.42755127, -34.3615762875, 151.424560547, -33.4543597895}},
        {"tokyo", {139.240722656, 35.2186974963, 140.498657227, 36.2575628263}},
        {"warszawa", {20.7202148438, 52.0322181041, 21.3024902344, 52.4091212523}},
        {"washington", {-77.4920654297, 38.5954071994, -76.6735839844, 39.2216149801}},
        {"wien", {16.0894775391, 48.0633965378, 16.6387939453, 48.3525987075}},
    };

    m2::PointD const pt(p->lon, p->lat);

    for (auto const & city : cities)
    {
      if (city.second.IsPointInside(pt))
        return city.first;
    }
    return string();
  }

  void PreprocessElement(OsmElement * p)
  {
    bool hasLayer = false;
    char const * layer = nullptr;

    bool isSubwayEntrance = false;
    bool isSubwayStation = false;

    TagProcessor(p).ApplyRules
    ({
      { "bridge", "yes", [&layer] { layer = "1"; }},
      { "tunnel", "yes", [&layer] { layer = "-1"; }},
      { "layer", "*", [&hasLayer] { hasLayer = true; }},

      { "railway", "subway_entrance", [&isSubwayEntrance] { isSubwayEntrance = true; }},

      /// @todo Unfortunatelly, it's not working in many cases (route=subway, transport=subway).
      /// Actually, it's better to process subways after feature types assignment.
      { "station", "subway", [&isSubwayStation] { isSubwayStation = true; }},
    });

    if (!hasLayer && layer)
      p->AddTag("layer", layer);

    // Tag 'city' is needed for correct selection of metro icons.
    if (isSubwayEntrance || isSubwayStation)
    {
      string const & city = MatchCity(p);
      if (!city.empty())
        p->AddTag("city", city);
    }
  }

  void PostprocessElement(OsmElement * p, FeatureParams & params)
  {
    static CachedTypes const types;

    if (!params.house.IsEmpty())
    {
      // Delete "entrance" type for house number (use it only with refs).
      // Add "address" type if we have house number but no valid types.
      if (params.PopExactType(types.Get(CachedTypes::ENTRANCE)))
      {
        params.name.Clear();
        // If we have address (house name or number), we should assign valid type.
        // There are a lot of features like this in Czech Republic.
        params.AddType(types.Get(CachedTypes::ADDRESS));
      }
    }

    bool highwayDone = false;
    bool subwayDone = false;
    bool railwayDone = false;

    // Get a copy of source types, because we will modify params in the loop;
    FeatureParams::TTypes const vTypes = params.m_Types;
    for (size_t i = 0; i < vTypes.size(); ++i)
    {
      if (!highwayDone && types.IsHighway(vTypes[i]))
      {
        TagProcessor(p).ApplyRules
        ({
          { "oneway", "yes", [&params] { params.AddType(types.Get(CachedTypes::ONEWAY)); }},
          { "oneway", "1", [&params] { params.AddType(types.Get(CachedTypes::ONEWAY)); }},
          { "oneway", "-1", [&params] { params.AddType(types.Get(CachedTypes::ONEWAY)); params.m_reverseGeometry = true; }},

          { "access", "private", [&params] { params.AddType(types.Get(CachedTypes::PRIVATE)); }},

          { "lit", "~", [&params] { params.AddType(types.Get(CachedTypes::LIT)); }},

          { "foot", "!", [&params] { params.AddType(types.Get(CachedTypes::NOFOOT)); }},

          { "foot", "~", [&params] { params.AddType(types.Get(CachedTypes::YESFOOT)); }},
          { "sidewalk", "~", [&params] { params.AddType(types.Get(CachedTypes::YESFOOT)); }},
        });

        highwayDone = true;
      }

      if (!subwayDone && types.IsRwSubway(vTypes[i]))
      {
        TagProcessor(p).ApplyRules
        ({
          { "network", "London Underground", [&params] { params.SetRwSubwayType("london"); }},
          { "network", "New York City Subway", [&params] { params.SetRwSubwayType("newyork"); }},
          { "network", "Московский метрополитен", [&params] { params.SetRwSubwayType("moscow"); }},
          { "network", "Петербургский метрополитен", [&params] { params.SetRwSubwayType("spb"); }},
          { "network", "Verkehrsverbund Berlin-Brandenburg", [&params] { params.SetRwSubwayType("berlin"); }},
          { "network", "Минский метрополитен", [&params] { params.SetRwSubwayType("minsk"); }},

          { "network", "Київський метрополітен", [&params] { params.SetRwSubwayType("kiev"); }},
          { "operator", "КП «Київський метрополітен»", [&params] { params.SetRwSubwayType("kiev"); }},

          { "network", "RATP", [&params] { params.SetRwSubwayType("paris"); }},
          { "network", "Metro de Barcelona", [&params] { params.SetRwSubwayType("barcelona"); }},

          { "network", "Metro de Madrid", [&params] { params.SetRwSubwayType("madrid"); }},
          { "operator", "Metro de Madrid", [&params] { params.SetRwSubwayType("madrid"); }},

          { "network", "Metropolitana di Roma", [&params] { params.SetRwSubwayType("roma"); }},
          { "network", "ATAC", [&params] { params.SetRwSubwayType("roma"); }},
        });

        subwayDone = true;
      }

      if (!subwayDone && !railwayDone && types.IsRwStation(vTypes[i]))
      {
        TagProcessor(p).ApplyRules
        ({
          { "network", "London Underground", [&params] { params.SetRwSubwayType("london"); }},
        });

        railwayDone = true;
      }
    }
  }

  void GetNameAndType(OsmElement * p, FeatureParams & params)
  {
    // Stage1: Preprocess tags.
    PreprocessElement(p);

    // Stage2: Process feature name on all languages.
    ForEachTag<bool>(p, NamesExtractor(params));

    // Stage3: Process base feature tags.
    TagProcessor(p).ApplyRules<void(string &, string &)>
    ({
      { "atm", "yes", [](string & k, string & v) { k.swap(v); k = "amenity"; }},
      { "restaurant", "yes", [](string & k, string & v) { k.swap(v); k = "amenity"; }},
      { "hotel", "yes", [](string & k, string & v) { k.swap(v); k = "tourism"; }},
      { "building", "entrance", [](string & k, string & v) { k.swap(v); v = "yes"; }},

      { "addr:city", "*", [&params](string & k, string & v) { params.AddPlace(Normalize(v)); k.clear(); v.clear(); }},
      { "addr:place", "*", [&params](string & k, string & v) { params.AddPlace(Normalize(v)); k.clear(); v.clear(); }},
      { "addr:housenumber", "*", [&params](string & k, string & v) { params.AddHouseName(Normalize(v)); k.clear(); v.clear(); }},
      { "addr:housename", "*", [&params](string & k, string & v) { params.AddHouseName(Normalize(v)); k.clear(); v.clear(); }},
      { "addr:street", "*", [&params](string & k, string & v) { params.AddStreet(Normalize(v)); k.clear(); v.clear(); }},
      //{ "addr:streetnumber", "*", [&params](string & k, string & v) { params.AddStreet(Normalize(v)); k.clear(); v.clear(); }},
      //{ "addr:full", "*", [&params](string & k, string & v) { params.AddAddress(Normalize(v)); k.clear(); v.clear(); }},
      { "addr:postcode", "*", [&params](string & k, string & v) { params.AddPostcode(Normalize(v)); k.clear(); v.clear(); }},
      { "addr:flats", "*", [&params](string & k, string & v) { params.flats = v; k.clear(); v.clear(); }},

      { "population", "*", [&params](string & k, string & v)
        {
          // Get population rank.
          // TODO: similar formula with indexer/feature.cpp, possible need refactoring
          uint64_t n;
          if (strings::to_uint64(v, n))
            params.rank = static_cast<uint8_t>(log(double(n)) / log(1.1));
          k.clear(); v.clear();
        }},
      { "ref", "*", [&params](string & k, string & v)
        {
          // Get reference (we process road numbers only).
          params.ref = v;
          k.clear(); v.clear();
        }},
      { "layer", "*", [&params](string & k, string & v)
        {
          // Get layer.
          if (params.layer == 0)
          {
            params.layer = atoi(v.c_str());
            int8_t const bound = 10;
            params.layer = my::clamp(params.layer, -bound, bound);
          }
        }},
    });

    // Stage4: Match tags in classificator to find feature types.
    MatchTypes(p, params);

    // Stage5: Postprocess feature types.
    PostprocessElement(p, params);

    params.FinishAddingTypes();

    // Stage6: Collect addidtional information about feature such as
    // hotel stars, opening hours, cuisine, ...
    ForEachTag<bool>(p, MetadataTagProcessor(params));
  }
}
