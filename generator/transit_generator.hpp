#pragma once

#include "generator/osm_id.hpp"

#include "routing_common/transit_types.hpp"

#include "geometry/point2d.hpp"

#include "base/macros.hpp"
#include "base/string_utils.hpp"

#include "3party/jansson/myjansson.hpp"

#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace routing
{
namespace transit
{
using OsmIdToFeatureIdsMap = std::map<osm::Id, std::vector<FeatureId>>;

class DeserializerFromJson
{
public:
  DeserializerFromJson(json_struct_t* node, std::shared_ptr<OsmIdToFeatureIdsMap> const & osmIdToFeatureIds);

  template<typename T>
  typename std::enable_if<std::is_integral<T>::value || std::is_enum<T>::value || std::is_same<T, double>::value>::type
      operator()(T & t, char const * name = nullptr)
  {
    if (name == nullptr || strcmp(name, "osm_id") != 0)
    {
      GetField(t, name);
      return;
    }

    // Conversion osm id to feature id.
    std::string osmIdStr;
    GetField(osmIdStr, name);
    CHECK(strings::is_number(osmIdStr), ());
    uint64_t osmIdNum;
    CHECK(strings::to_uint64(osmIdStr.c_str(), osmIdNum), ());
    osm::Id const osmId(osmIdNum);
    auto const it = m_osmIdToFeatureIds->find(osm::Id(osmIdNum));
    CHECK(it != m_osmIdToFeatureIds->cend(), ());
    CHECK_EQUAL(it->second.size(), 1,
        ("Osm id:", osmId, "from transit graph doesn't present by a single feature in mwm."));
    t = static_cast<T>(it->second[0]);
  }

  void operator()(std::string & s, char const * name = nullptr) { GetField(s, name); }
  void operator()(m2::PointD & p, char const * name = nullptr);

  template <typename T>
  void operator()(std::vector<T> & vs, char const * name = nullptr)
  {
    auto * arr = my::GetJSONOptionalField(m_node, name);
    if (arr == nullptr)
      return;

    if (!json_is_array(arr))
      MYTHROW(my::Json::Exception, ("The field", name, "must contain a json array."));
    size_t const sz = json_array_size(arr);
    vs.resize(sz);
    for (size_t i = 0; i < sz; ++i)
    {
      DeserializerFromJson arrayItem(json_array_get(arr, i), m_osmIdToFeatureIds);
      arrayItem(vs[i]);
    }
  }

  template<typename T>
  typename std::enable_if<std::is_class<T>::value>::type operator()(T & t, char const * name = nullptr)
  {
    t.Visit(*this);
  }

private:
  template <typename T>
  void GetField(T & t, char const * name = nullptr)
  {
    if (name == nullptr)
    {
      // |name| is not set in case of array items
      FromJSON(m_node, t);
      return;
    }

    json_struct_t * field = my::GetJSONOptionalField(m_node, name);
    if (field == nullptr)
    {
      // No optional field |name| at |m_node|. In that case the default value should be set to |t|.
      // This default value is set at constructor of corresponding class which is filled with
      // |DeserializerFromJson|. And the value (|t|) is not changed at this method.
      return;
    }
    FromJSON(field, t);
  }

  json_struct_t * m_node;
  std::shared_ptr<OsmIdToFeatureIdsMap> m_osmIdToFeatureIds;
};

/// \brief Builds the transit section in the mwm.
/// \param mwmPath relative or full path to an mwm. The name of mwm without extension is considered
/// as country id.
/// \param transitDir a path to directory with json files with transit graphs.
/// \note An mwm pointed by |mwmPath| should contain:
/// * feature geometry
/// * index graph (ROUTING_FILE_TAG)
void BuildTransit(std::string const & mwmPath, std::string const & osmIdsToFeatureIdPath,
                  std::string const & transitDir);
}  // namespace transit
}  // namespace routing
