#pragma once

#include "indexer/feature_data.hpp"
#include "indexer/feature_decl.hpp"
#include "indexer/feature_meta.hpp"
#include "indexer/map_object.hpp"

#include "geometry/latlon.hpp"
#include "geometry/mercator.hpp"

#include "coding/multilang_utf8_string.hpp"

#include "std/vector.hpp"

namespace osm
{
/// Holds information to construct editor's UI.
struct EditableProperties
{
  EditableProperties() = default;
  EditableProperties(vector<feature::Metadata::EType> const & metadata,
                     bool name, bool address)
      : m_name(name),
        m_address(address),
        m_metadata(metadata)
  {
  }

  bool m_name = false;
  /// If true, enables editing of house number, street address and post code.
  bool m_address = false;
  vector<feature::Metadata::EType> m_metadata;
  bool IsEditable() const { return m_name || m_address || !m_metadata.empty(); }
};

struct LocalizedName
{
  LocalizedName() = default;
  LocalizedName(int8_t code, string const & name);
  LocalizedName(string const & langCode, string const & name);

  // m_code, m_lang and m_langName are defined in StringUtf8Multilang.
  int8_t const m_code;
  // Non-owning pointers to internal static char const * array.
  char const * const m_lang;
  char const * const m_langName;
  string const m_name;
};

struct LocalizedStreet
{
  string m_defaultName;
  string m_localizedName;

  bool operator==(LocalizedStreet const & st) const { return m_defaultName == st.m_defaultName; }
};

class EditableMapObject : public MapObject
{
public:
  static int8_t const kMaximumLevelsEditableByUsers;

  bool IsNameEditable() const;
  bool IsAddressEditable() const;

  vector<Props> GetEditableProperties() const;
  // TODO(AlexZ): Remove this method and use GetEditableProperties() in UI.
  vector<feature::Metadata::EType> const & GetEditableFields() const;

  StringUtf8Multilang const & GetName() const;
  vector<LocalizedName> GetLocalizedNames() const;
  LocalizedStreet const & GetStreet() const;
  vector<LocalizedStreet> const & GetNearbyStreets() const;
  string const & GetHouseNumber() const;
  string GetPostcode() const;
  string GetWikipedia() const;

  void SetEditableProperties(osm::EditableProperties const & props);
  //  void SetFeatureID(FeatureID const & fid);
  void SetName(StringUtf8Multilang const & name);
  void SetName(string const & name, int8_t langCode = StringUtf8Multilang::kDefaultCode);
  void SetMercator(m2::PointD const & center);
  void SetType(uint32_t featureType);
  void SetID(FeatureID const & fid);

  //  void SetTypes(feature::TypesHolder const & types);
  void SetStreet(LocalizedStreet const & st);
  void SetNearbyStreets(vector<LocalizedStreet> && streets);
  void SetHouseNumber(string const & houseNumber);
  void SetPostcode(string const & postcode);
  void SetPhone(string const & phone);
  void SetFax(string const & fax);

  void SetEmail(string const & email);
  void SetWebsite(string website);
  void SetWikipedia(string const & wikipedia);

  void SetInternet(Internet internet);
  void SetStars(int stars);
  void SetOperator(string const & op);

  void SetElevation(double ele);
  void SetFlats(string const & flats);

  void SetBuildingLevels(string const & buildingLevels);
  /// @param[in] cuisine is a vector of osm cuisine ids.
  void SetCuisines(vector<string> const & cuisine);
  void SetOpeningHours(string const & openingHours);

  /// Special mark that it's a point feature, not area or line.
  void SetPointType();

  static bool ValidateBuildingLevels(string const & buildingLevels);
  static bool ValidateHouseNumber(string const & houseNumber);
  static bool ValidateFlats(string const & flats);
  static bool ValidatePostCode(string const & postCode);
  static bool ValidatePhone(string const & phone);
  static bool ValidateWebsite(string const & site);
  static bool ValidateEmail(string const & email);

private:
  string m_houseNumber;
  LocalizedStreet m_street;
  vector<LocalizedStreet> m_nearbyStreets;
  EditableProperties m_editableProperties;
};
}  // namespace osm
