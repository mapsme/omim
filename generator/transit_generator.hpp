#pragma once

#include "geometry/point2d.hpp"

#include "base/macros.hpp"

#include "3party/jansson/myjansson.hpp"

#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

namespace routing
{
namespace transit
{
class DeserializerFromJson
{
public:
  DeserializerFromJson(json_struct_t * node) : m_node(node) {}

  template<typename T>
  typename std::enable_if<std::is_integral<T>::value || std::is_enum<T>::value || std::is_same<T, double>::value>::type
      operator()(T & t, char const * name = nullptr)
  {
    GetField(t, name);
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
      DeserializerFromJson arrayItem(json_array_get(arr, i));
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
};

/// \brief Builds the transit section in the mwm.
/// \param mwmPath relative or full path to an mwm. The name of mwm without extension is considered
/// as country id.
/// \param transitDir a path to directory with json files with transit graphs.
void BuildTransit(std::string const & mwmPath, std::string const & transitDir);
}  // namespace transit
}  // namespace routing
