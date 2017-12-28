#include "generator/osm2type.hpp"
#include "generator/osm2meta.hpp"
#include "generator/osm_element.hpp"

#include "indexer/cities.hpp"
#include "indexer/classificator.hpp"
#include "indexer/feature_impl.hpp"
#include "indexer/feature_visibility.hpp"

#include "geometry/mercator.hpp"

#include "base/assert.hpp"
#include "base/stl_add.hpp"
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

      my::StringIL arr[] =
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
        // Prevent merging different tags (e.g. shop=pet from shop=abandoned, was:shop=pet).
       ClassifObjectPtr pObj =
           path.size() == 1 ? ClassifObjectPtr()
                            : ForEachTagEx<ClassifObjectPtr>(
                                  p, skipRows, [&current](string const & k, string const & v) {
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
    m2::PointD const pt(p->lon, p->lat);
    feature::ECity city = feature::MatchCity(pt);
    if (city != feature::ECity::NO_CITY)
      return string(1, static_cast<char>(city));
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

    static my::StringIL pavedSurfaces = {"paved", "asphalt", "cobblestone", "cobblestone:flattened",
                                         "sett", "concrete", "concrete:lanes", "concrete:plates",
                                         "paving_stones", "metal", "wood"};
    static my::StringIL badSurfaces = {"cobblestone", "sett", "metal", "wood", "grass", "gravel",
                                       "mud", "sand", "snow", "woodchips"};
    static my::StringIL badSmoothness = {"bad", "very_bad", "horrible", "very_horrible", "impassable",
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
      { "subway", "yes", [&isSubway] { isSubway = true; }},
      { "light_rail", "yes", [&isSubway] { isSubway = true; }},
      { "bus", "yes", [&isBus] { isBus = true; }},
      { "trolleybus", "yes", [&isBus] { isBus = true; }},
      { "tram", "yes", [&isTram] { isTram = true; }},

      /// @todo Unfortunatelly, it's not working in many cases (route=subway, transport=subway).
      /// Actually, it's better to process subways after feature types assignment.
      { "station", "subway", [&isSubway] { isSubway = true; }},
      { "station", "light_rail", [&isSubway] { isSubway = true; }},
    });

    if (!hasLayer && layer)
      p->AddTag("layer", layer);

    // Tag 'city' is needed for correct selection of metro icons.
    if (isSubway)
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
    FeatureParams::TTypes const vTypes = params.m_Types;
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
    MatchTypes(p, params);

    // Stage5: Postprocess feature types.
    PostprocessElement(p, params);

    params.FinishAddingTypes();

    // Stage6: Collect addidtional information about feature such as
    // hotel stars, opening hours, cuisine, ...
    ForEachTag<bool>(p, MetadataTagProcessor(params));
  }
}
