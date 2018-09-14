#include "generator/osm2type.hpp"
#include "generator/osm2meta.hpp"
#include "generator/osm_element.hpp"

#include "indexer/classificator.hpp"
#include "indexer/feature_impl.hpp"
#include "indexer/feature_visibility.hpp"

#include "geometry/mercator.hpp"

#include "base/assert.hpp"
#include "base/stl_helpers.hpp"
#include "base/string_utils.hpp"

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <set>
#include <vector>

using namespace std;

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
          // [wheelchair=no] should be processed
          {"wheelchair", false},
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

        // Do not consider languages with suffixes, like "en:pronunciation".
        if (++token)
          return false;

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

        m_params.AddName(lang, v);
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
            if (e.key != rule.key)
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
                 NOBICYCLE, YESBICYCLE, BICYCLE_BIDIR, SURFPGOOD, SURFPBAD, SURFUGOOD, SURFUBAD,
                 HASPARTS, NOCAR, YESCAR, WLAN, RW_STATION, RW_STATION_SUBWAY, WHEELCHAIR_YES,
                 BARRIER_GATE, TOLL
               };

    CachedTypes()
    {
      Classificator const & c = classif();

      base::StringIL arr[] =
      {
        {"entrance"}, {"highway"},
        {"building", "address"}, {"hwtag", "oneway"}, {"hwtag", "private"},
        {"hwtag", "lit"}, {"hwtag", "nofoot"}, {"hwtag", "yesfoot"},
        {"hwtag", "nobicycle"}, {"hwtag", "yesbicycle"}, {"hwtag", "bidir_bicycle"},
        {"psurface", "paved_good"}, {"psurface", "paved_bad"},
        {"psurface", "unpaved_good"}, {"psurface", "unpaved_bad"},
        {"building", "has_parts"}, {"hwtag", "nocar"}, {"hwtag", "yescar"},
        {"internet_access", "wlan"}, {"railway", "station"}, {"railway", "station", "subway"},
        {"wheelchair", "yes"}, {"barrier", "gate"}, {"hwtag", "toll"}
      };

      for (auto const & e : arr)
        m_types.push_back(c.GetTypeByPath(e));
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

  void MatchTypes(OsmElement * p, FeatureParams & params,
                  function<bool(uint32_t)> filterDrawableType)
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
        // Prevent merging different tags (e.g. shop=pet from shop=abandoned, was:shop=pet).

        ClassifObjectPtr pObj;
        if (path.size() != 1)
        {
          pObj = ForEachTagEx<ClassifObjectPtr>(
                   p, skipRows, [&current](string const & k, string const & v) {
                     return NeedMatchValue(k, v) ? current->BinaryFind(v) : ClassifObjectPtr();
                   });
        }

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

      if (filterDrawableType(t))
        params.AddType(t);
    } while (true);
  }

  string MatchCity(OsmElement const * p)
  {
    static map<string, m2::RectD> const cities = {
        {"almaty", {76.7223358154, 43.1480920701, 77.123336792, 43.4299852362}},
        {"amsterdam", {4.65682983398, 52.232846171, 5.10040283203, 52.4886341706}},
        {"baires", {-58.9910888672, -35.1221551064, -57.8045654297, -34.2685661867}},
        {"bangkok", {100.159606934, 13.4363737155, 100.909423828, 14.3069694978}},
        {"barcelona", {1.94458007812, 41.2489025224, 2.29614257812, 41.5414776668}},
        {"beijing", {115.894775391, 39.588757277, 117.026367187, 40.2795256688}},
        {"berlin", {13.0352783203, 52.3051199211, 13.7933349609, 52.6963610783}},
        {"boston", {-71.2676239014, 42.2117365893, -70.8879089355, 42.521711682}},
        {"brussel", {4.2448425293, 50.761653413, 4.52499389648, 50.9497757762}},
        {"budapest", {18.7509155273, 47.3034470439, 19.423828125, 47.7023684666}},
        {"chicago", {-88.3163452148, 41.3541338721, -87.1270751953, 42.2691794924}},
        {"delhi", {76.8026733398, 28.3914003758, 77.5511169434, 28.9240352884}},
        {"dnepro", {34.7937011719, 48.339820521, 35.2798461914, 48.6056737841}},
        {"dubai", {55.01953125, 24.9337667594, 55.637512207, 25.6068559937}},
        {"ekb", {60.3588867188, 56.6622647682, 61.0180664062, 57.0287738515}},
        {"frankfurt", {8.36334228516, 49.937079757, 8.92364501953, 50.2296379179}},
        {"guangzhou", {112.560424805, 22.4313401564, 113.766174316, 23.5967112789}},
        {"hamburg", {9.75860595703, 53.39151869, 10.2584838867, 53.6820686709}},
        {"helsinki", {24.3237304688, 59.9989861206, 25.48828125, 60.44638186}},
        {"hongkong", {114.039459229, 22.1848617608, 114.305877686, 22.3983322415}},
        {"kazan", {48.8067626953, 55.6372985742, 49.39453125, 55.9153515154}},
        {"kiev", {30.1354980469, 50.2050332649, 31.025390625, 50.6599083609}},
        {"koln", {6.7943572998, 50.8445380881, 7.12669372559, 51.0810964366}},
        {"lima", {-77.2750854492, -12.3279274859, -76.7999267578, -11.7988014362}},
        {"lisboa", {-9.42626953125, 38.548165423, -8.876953125, 38.9166815364}},
        {"london", {-0.4833984375, 51.3031452592, 0.2197265625, 51.6929902115}},
        {"madrid", {-4.00451660156, 40.1536868578, -3.32885742188, 40.6222917831}},
        {"mexico", {-99.3630981445, 19.2541083164, -98.879699707, 19.5960192403}},
        {"milan", {9.02252197266, 45.341528405, 9.35760498047, 45.5813674681}},
        {"minsk", {27.2845458984, 53.777934972, 27.8393554688, 54.0271334441}},
        {"moscow", {36.9964599609, 55.3962717136, 38.1884765625, 56.1118730004}},
        {"mumbai", {72.7514648437, 18.8803004445, 72.9862976074, 19.2878132403}},
        {"munchen", {11.3433837891, 47.9981928195, 11.7965698242, 48.2530267576}},
        {"newyork", {-74.4104003906, 40.4134960497, -73.4600830078, 41.1869224229}},
        {"nnov", {43.6431884766, 56.1608472541, 44.208984375, 56.4245355509}},
        {"novosibirsk", {82.4578857422, 54.8513152597, 83.2983398438, 55.2540770671}},
        {"osaka", {134.813232422, 34.1981730963, 136.076660156, 35.119908571}},
        {"oslo", {10.3875732422, 59.7812868211, 10.9286499023, 60.0401604652}},
        {"paris", {2.09014892578, 48.6637569323, 2.70538330078, 49.0414689141}},
        {"rio", {-43.4873199463, -23.0348745407, -43.1405639648, -22.7134898498}},
        {"roma", {12.3348999023, 41.7672146942, 12.6397705078, 42.0105298189}},
        {"sanfran", {-122.72277832, 37.1690715771, -121.651611328, 38.0307856938}},
        {"santiago", {-71.015625, -33.8133843291, -70.3372192383, -33.1789392606}},
        {"saopaulo", {-46.9418334961, -23.8356009866, -46.2963867187, -23.3422558351}},
        {"seoul", {126.540527344, 37.3352243593, 127.23815918, 37.6838203267}},
        {"shanghai", {119.849853516, 30.5291450367, 122.102050781, 32.1523618947}},
        {"shenzhen", {113.790893555, 22.459263801, 114.348449707, 22.9280416657}},
        {"singapore", {103.624420166, 1.21389843409, 104.019927979, 1.45278619819}},
        {"spb", {29.70703125, 59.5231755354, 31.3110351562, 60.2725145948}},
        {"stockholm", {17.5726318359, 59.1336814082, 18.3966064453, 59.5565918857}},
        {"stuttgart", {9.0877532959, 48.7471343254, 9.29306030273, 48.8755544436}},
        {"sydney", {150.42755127, -34.3615762875, 151.424560547, -33.4543597895}},
        {"taipei", {121.368713379, 24.9312761454, 121.716156006, 25.1608229799}},
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

  string DetermineSurface(OsmElement * p)
  {
    string surface;
    string smoothness;
    string surface_grade;
    bool isHighway = false;

    for (auto const & tag : p->m_tags)
    {
      if (tag.key == "surface")
        surface = tag.value;
      else if (tag.key == "smoothness")
        smoothness = tag.value;
      else if (tag.key == "surface:grade")
        surface_grade = tag.value;
      else if (tag.key == "highway")
        isHighway = true;
    }

    if (!isHighway || (surface.empty() && smoothness.empty()))
      return string();

    static base::StringIL pavedSurfaces = {"paved", "asphalt", "cobblestone", "cobblestone:flattened",
                                           "sett", "concrete", "concrete:lanes", "concrete:plates",
                                           "paving_stones", "metal", "wood"};
    static base::StringIL badSurfaces = {"cobblestone", "sett", "metal", "wood", "grass", "gravel",
                                         "mud", "sand", "snow", "woodchips"};
    static base::StringIL badSmoothness = {"bad", "very_bad", "horrible", "very_horrible", "impassable",
                                           "robust_wheels", "high_clearance", "off_road_wheels", "rough"};

    bool isPaved = false;
    bool isGood = true;

    if (!surface.empty())
    {
      for (auto const & value : pavedSurfaces)
        if (surface == value)
          isPaved = true;
    }
    else
      isPaved = smoothness == "excellent" || smoothness == "good";

    if (!smoothness.empty())
    {
      for (auto const & value : badSmoothness)
        if (smoothness == value)
          isGood = false;
      if (smoothness == "bad" && !isPaved)
        isGood = true;
    }
    else if (surface_grade == "0" || surface_grade == "1")
      isGood = false;
    else
    {
      if (surface_grade != "3" )
        for (auto const & value : badSurfaces)
          if (surface == value)
            isGood = false;
    }

    string psurface = isPaved ? "paved_" : "unpaved_";
    psurface += isGood ? "good" : "bad";
    return psurface;
  }

  void PreprocessElement(OsmElement * p)
  {
    bool hasLayer = false;
    char const * layer = nullptr;

    bool isSubway = false;
    bool isBus = false;
    bool isTram = false;

    TagProcessor(p).ApplyRules
    ({
      { "bridge", "yes", [&layer] { layer = "1"; }},
      { "tunnel", "yes", [&layer] { layer = "-1"; }},
      { "layer", "*", [&hasLayer] { hasLayer = true; }},

      { "railway", "subway_entrance", [&isSubway] { isSubway = true; }},
      { "bus", "yes", [&isBus] { isBus = true; }},
      { "trolleybus", "yes", [&isBus] { isBus = true; }},
      { "tram", "yes", [&isTram] { isTram = true; }},

      /// @todo Unfortunatelly, it's not working in many cases (route=subway, transport=subway).
      /// Actually, it's better to process subways after feature types assignment.
      { "station", "subway", [&isSubway] { isSubway = true; }},
    });

    if (!hasLayer && layer)
      p->AddTag("layer", layer);

    // Tag 'city' is needed for correct selection of metro icons.
    if (isSubway && p->type == OsmElement::EntityType::Node)
    {
      string const city = MatchCity(p);
      if (!city.empty())
        p->AddTag("city", city);
    }

    p->AddTag("psurface", DetermineSurface(p));

    // Convert public_transport tags to the older schema.
    for (auto const & tag : p->m_tags)
    {
      if (tag.key == "public_transport")
      {
        if (tag.value == "platform" && isBus)
        {
          if (p->type == OsmElement::EntityType::Node)
            p->AddTag("highway", "bus_stop");
          else
            p->AddTag("highway", "platform");
        }
        else if (tag.value == "stop_position" && isTram && p->type == OsmElement::EntityType::Node)
          p->AddTag("railway", "tram_stop");
        break;
      }
    }

    // Merge attraction and memorial types to predefined set of values
    p->UpdateTag("artwork_type", [](string & value) {
      if (value.empty())
        return;
      if (value == "mural" || value == "graffiti" || value == "azulejo" || value == "tilework")
        value = "painting";
      else if (value == "stone" || value == "installation")
        value = "sculpture";
      else if (value == "bust")
        value = "statue";
    });

    string const & memorialType = p->GetTag("memorial:type");
    p->UpdateTag("memorial", [&memorialType](string & value) {
      if (value.empty()) {
        if (memorialType.empty())
          return;
        else
          value = memorialType;
      }

      if (value == "blue_plaque" || value == "stolperstein")
        value = "plaque";
      else if (value == "war_memorial" || value == "stele" || value == "obelisk" ||
               value == "stone" || value == "cross")
        value = "sculpture";
      else if (value == "bust" || value == "person")
        value = "statue";
    });

    p->UpdateTag("castle_type", [](string & value) {
      if (value.empty())
        return;
      if (value == "fortress" || value == "kremlin" || value == "castrum" ||
          value == "shiro" || value == "citadel")
        value = "defensive";
      else if (value == "manor" || value == "palace")
        value = "stately";
    });

    p->UpdateTag("attraction", [](string & value) {
      // "specified" is a special value which means we have the "attraction" tag,
      // but its value is not "animal".
      if (!value.empty() && value != "animal")
        value = "specified";
    });
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

    // Process yes/no tags.
    TagProcessor(p).ApplyRules
    ({
      { "wheelchair", "designated", [&params] { params.AddType(types.Get(CachedTypes::WHEELCHAIR_YES)); }},
      { "wifi", "~", [&params] { params.AddType(types.Get(CachedTypes::WLAN)); }},
      { "building:part", "no", [&params] { params.AddType(types.Get(CachedTypes::HASPARTS)); }},
      { "building:parts", "~", [&params] { params.AddType(types.Get(CachedTypes::HASPARTS)); }},
    });

    bool highwayDone = false;
    bool subwayDone = false;
    bool railwayDone = false;

    bool addOneway = false;
    bool noOneway = false;

    // Get a copy of source types, because we will modify params in the loop;
    FeatureParams::Types const vTypes = params.m_types;
    for (size_t i = 0; i < vTypes.size(); ++i)
    {
      if (!highwayDone && types.IsHighway(vTypes[i]))
      {
        TagProcessor(p).ApplyRules
        ({
          { "oneway", "yes", [&addOneway] { addOneway = true; }},
          { "oneway", "1", [&addOneway] { addOneway = true; }},
          { "oneway", "-1", [&addOneway, &params] { addOneway = true; params.m_reverseGeometry = true; }},
          { "oneway", "!", [&noOneway] { noOneway = true; }},
          { "junction", "roundabout", [&addOneway] { addOneway = true; }},

          { "access", "private", [&params] { params.AddType(types.Get(CachedTypes::PRIVATE)); }},
          { "access", "!", [&params] { params.AddType(types.Get(CachedTypes::PRIVATE)); }},

          { "barrier", "gate", [&params] { params.AddType(types.Get(CachedTypes::BARRIER_GATE)); }},

          { "lit", "~", [&params] { params.AddType(types.Get(CachedTypes::LIT)); }},
          { "toll", "~", [&params] { params.AddType(types.Get(CachedTypes::TOLL)); }},

          { "foot", "!", [&params] { params.AddType(types.Get(CachedTypes::NOFOOT)); }},
          { "foot", "use_sidepath", [&params] { params.AddType(types.Get(CachedTypes::NOFOOT)); }},
          { "foot", "~", [&params] { params.AddType(types.Get(CachedTypes::YESFOOT)); }},
          { "sidewalk", "~", [&params] { params.AddType(types.Get(CachedTypes::YESFOOT)); }},

          { "bicycle", "!", [&params] { params.AddType(types.Get(CachedTypes::NOBICYCLE)); }},
          { "bicycle", "~", [&params] { params.AddType(types.Get(CachedTypes::YESBICYCLE)); }},
          { "cycleway", "~", [&params] { params.AddType(types.Get(CachedTypes::YESBICYCLE)); }},
          { "cycleway:right", "~", [&params] { params.AddType(types.Get(CachedTypes::YESBICYCLE)); }},
          { "cycleway:left", "~", [&params] { params.AddType(types.Get(CachedTypes::YESBICYCLE)); }},
          { "oneway:bicycle", "!", [&params] { params.AddType(types.Get(CachedTypes::BICYCLE_BIDIR)); }},
          { "cycleway", "opposite", [&params] { params.AddType(types.Get(CachedTypes::BICYCLE_BIDIR)); }},

          { "motor_vehicle", "private", [&params] { params.AddType(types.Get(CachedTypes::NOCAR)); }},
          { "motor_vehicle", "!", [&params] { params.AddType(types.Get(CachedTypes::NOCAR)); }},
          { "motor_vehicle", "yes", [&params] { params.AddType(types.Get(CachedTypes::YESCAR)); }},
          { "motorcar", "private", [&params] { params.AddType(types.Get(CachedTypes::NOCAR)); }},
          { "motorcar", "!", [&params] { params.AddType(types.Get(CachedTypes::NOCAR)); }},
          { "motorcar", "yes", [&params] { params.AddType(types.Get(CachedTypes::YESCAR)); }},
        });

        if (addOneway && !noOneway)
          params.AddType(types.Get(CachedTypes::ONEWAY));

        highwayDone = true;
      }

      /// @todo Probably, we can delete this processing because cities
      /// are matched by limit rect in MatchCity.
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

  void GetNameAndType(OsmElement * p, FeatureParams & params,
                      function<bool(uint32_t)> filterDrawableType)
  {
    // Stage1: Preprocess tags.
    PreprocessElement(p);

    // Stage2: Process feature name on all languages.
    ForEachTag<bool>(p, NamesExtractor(params));

    // Stage3: Process base feature tags.
    TagProcessor(p).ApplyRules<void(string &, string &)>
    ({
      { "addr:city", "*", [&params](string & k, string & v) { params.AddPlace(v); k.clear(); v.clear(); }},
      { "addr:place", "*", [&params](string & k, string & v) { params.AddPlace(v); k.clear(); v.clear(); }},
      { "addr:housenumber", "*", [&params](string & k, string & v) { params.AddHouseName(v); k.clear(); v.clear(); }},
      { "addr:housename", "*", [&params](string & k, string & v) { params.AddHouseName(v); k.clear(); v.clear(); }},
      { "addr:street", "*", [&params](string & k, string & v) { params.AddStreet(v); k.clear(); v.clear(); }},
      //{ "addr:streetnumber", "*", [&params](string & k, string & v) { params.AddStreet(v); k.clear(); v.clear(); }},
      // This line was first introduced by vng and was never used uncommented.
      //{ "addr:full", "*", [&params](string & k, string & v) { params.AddAddress(v); k.clear(); v.clear(); }},

      // addr:postcode must be passed to the metadata processor.
      // { "addr:postcode", "*", [&params](string & k, string & v) { params.AddPostcode(v); k.clear(); v.clear(); }},

      { "population", "*", [&params](string & k, string & v)
        {
          // Get population rank.
          uint64_t n;
          if (strings::to_uint64(v, n))
            params.rank = feature::PopulationToRank(n);
          k.clear(); v.clear();
        }
      },
      { "ref", "*", [&params](string & k, string & v)
        {
          // Get reference (we process road numbers only).
          params.ref = v;
          k.clear(); v.clear();
        }
      },
      { "layer", "*", [&params](string & /* k */, string & v)
        {
          // Get layer.
          if (params.layer == 0)
          {
            params.layer = atoi(v.c_str());
            int8_t const bound = 10;
            params.layer = my::clamp(params.layer, static_cast<int8_t>(-bound), bound);
          }
        }
      },
    });

    // Stage4: Match tags in classificator to find feature types.
    MatchTypes(p, params, filterDrawableType);

    // Stage5: Postprocess feature types.
    PostprocessElement(p, params);

    params.FinishAddingTypes();

    // Stage6: Collect additional information about feature such as
    // hotel stars, opening hours, cuisine, ...
    ForEachTag<bool>(p, MetadataTagProcessor(params));
  }
}
