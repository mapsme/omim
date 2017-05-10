#pragma once

#include "geometry/mercator.hpp"
#include "geometry/point2d.hpp"

#include "coding/multilang_utf8_string.hpp"

#include "base/string_utils.hpp"

#include <ctime>
#include <iostream>
#include <vector>

#include "3party/pugixml/src/pugixml.hpp"

namespace editor
{
DECLARE_EXCEPTION(XMLFeatureError, RootException);
DECLARE_EXCEPTION(InvalidXML, XMLFeatureError);
DECLARE_EXCEPTION(NoLatLon, XMLFeatureError);
DECLARE_EXCEPTION(NoXY, XMLFeatureError);
DECLARE_EXCEPTION(NoTimestamp, XMLFeatureError);
DECLARE_EXCEPTION(NoHeader, XMLFeatureError);

class XMLFeature
{
  static constexpr char const * kDefaultName = "name";
  static constexpr char const * kIntlName = "int_name";
  static constexpr char const * kLocalName = "name:";
  static char const * const kDefaultLang;
  static char const * const kIntlLang;

public:
  // Used in point to string serialization.
  static constexpr int kLatLonTolerance = 7;

  enum class Type
  {
    Node,
    Way
  };

  using TMercatorGeometry = std::vector<m2::PointD>;

  /// Creates empty node or way.
  XMLFeature(Type const type);
  XMLFeature(std::string const & xml);
  XMLFeature(pugi::xml_document const & xml);
  XMLFeature(pugi::xml_node const & xml);
  XMLFeature(XMLFeature const & feature);
  // TODO: It should make "deep" compare instead of converting to strings.
  // Strings comparison does not work if tags order is different but tags are equal.
  bool operator==(XMLFeature const & other) const;
  /// @returns nodes and ways from osmXml. Vector can be empty.
  static std::vector<XMLFeature> FromOSM(std::string const & osmXml);

  void Save(std::ostream & ost) const;
  std::string ToOSMString() const;

  /// Tags from featureWithChanges are applied to this(osm) feature.
  void ApplyPatch(XMLFeature const & featureWithChanges);

  Type GetType() const;
  std::string GetTypeString() const;

  /// @returns true only if it is a way and it is closed (area).
  bool IsArea() const;

  m2::PointD GetMercatorCenter() const;
  ms::LatLon GetCenter() const;
  void SetCenter(ms::LatLon const & ll);
  void SetCenter(m2::PointD const & mercatorCenter);

  TMercatorGeometry GetGeometry() const;

  /// Sets geometry in mercator to match against FeatureType's geometry in mwm
  /// when megrating to a new mwm build.
  /// Geometry points are now stored in <nd x="..." y="..." /> nodes like in osm <way>.
  /// But they are not the same as osm's. I.e. osm's one stores reference to a <node>
  /// with it's own data and lat, lon. Here we store only cooridanes in mercator.
  template <typename TIterator>
  void SetGeometry(TIterator begin, TIterator end)
  {
    ASSERT_EQUAL(GetType(), Type::Way, ("Only ways have geometry"));
    for (; begin != end; ++begin)
    {
      auto nd = GetRootNode().append_child("nd");
      nd.append_attribute("x") = strings::to_string_dac(begin->x, kLatLonTolerance).data();
      nd.append_attribute("y") = strings::to_string_dac(begin->y, kLatLonTolerance).data();
    }
  }

  template <typename TCollection>
  void SetGeometry(TCollection const & geometry)
  {
    SetGeometry(begin(geometry), end(geometry));
  }

  std::string GetName(std::string const & lang) const;
  std::string GetName(uint8_t const langCode = StringUtf8Multilang::kDefaultCode) const;

  template <typename TFunc>
  void ForEachName(TFunc && func) const
  {
    static auto const kPrefixLen = strlen(kLocalName);
    auto const tags = GetRootNode().select_nodes("tag");
    for (auto const & tag : tags)
    {
      std::string const & key = tag.node().attribute("k").value();

      if (strings::StartsWith(key, kLocalName))
        func(key.substr(kPrefixLen), tag.node().attribute("v").value());
      else if (key == kDefaultName)
        func(kDefaultLang, tag.node().attribute("v").value());
    }
  }

  void SetName(std::string const & name);
  void SetName(std::string const & lang, std::string const & name);
  void SetName(uint8_t const langCode, std::string const & name);

  std::string GetHouse() const;
  void SetHouse(std::string const & house);

  /// Our and OSM modification time are equal.
  time_t GetModificationTime() const;
  void SetModificationTime(time_t const time);

  /// @name XML storage format helpers.
  //@{
  uint32_t GetMWMFeatureIndex() const;
  void SetMWMFeatureIndex(uint32_t index);

  /// @returns my::INVALID_TIME_STAMP if there were no any upload attempt.
  time_t GetUploadTime() const;
  void SetUploadTime(time_t const time);

  std::string GetUploadStatus() const;
  void SetUploadStatus(std::string const & status);

  std::string GetUploadError() const;
  void SetUploadError(std::string const & error);
  //@}

  bool HasAnyTags() const;
  bool HasTag(std::string const & key) const;
  bool HasAttribute(std::string const & key) const;
  bool HasKey(std::string const & key) const;

  template <typename TFunc>
  void ForEachTag(TFunc && func) const
  {
    for (auto const & tag : GetRootNode().select_nodes("tag"))
      func(tag.node().attribute("k").value(), tag.node().attribute("v").value());
  }

  std::string GetTagValue(std::string const & key) const;
  void SetTagValue(std::string const & key, std::string value);

  std::string GetAttribute(std::string const & key) const;
  void SetAttribute(std::string const & key, std::string const & value);

  bool AttachToParentNode(pugi::xml_node parent) const;

private:
  pugi::xml_node const GetRootNode() const;
  pugi::xml_node GetRootNode();

  pugi::xml_document m_document;
};

std::string DebugPrint(XMLFeature const & feature);
std::string DebugPrint(XMLFeature::Type const type);
} // namespace editor
