#include "map/bookmark_helpers.hpp"

#include "kml/serdes.hpp"
#include "kml/serdes_binary.hpp"

#include "indexer/categories_holder.hpp"
#include "indexer/classificator.hpp"
#include "indexer/feature_utils.hpp"

#include "platform/preferred_languages.hpp"

#include "coding/file_reader.hpp"
#include "coding/file_writer.hpp"

#include <map>

namespace
{
std::map<std::string, kml::BookmarkIcon> const kBookmarkTypeToIcon = {
  {"amenity-bbq", kml::BookmarkIcon::Food},
  {"tourism-picnic_site", kml::BookmarkIcon::Food},
  {"leisure-picnic_table", kml::BookmarkIcon::Food},
  {"amenity-cafe", kml::BookmarkIcon::Food},
  {"amenity-restaurant", kml::BookmarkIcon::Food},
  {"amenity-fast_food", kml::BookmarkIcon::Food},
  {"amenity-food_court", kml::BookmarkIcon::Food},
  {"amenity-bar", kml::BookmarkIcon::Food},
  {"amenity-pub", kml::BookmarkIcon::Food},
  {"amenity-biergarten", kml::BookmarkIcon::Food},

  {"waterway-waterfall", kml::BookmarkIcon::Sights},
  {"historic-tomb", kml::BookmarkIcon::Sights},
  {"historic-boundary_stone", kml::BookmarkIcon::Sights},
  {"historic-ship", kml::BookmarkIcon::Sights},
  {"historic-archaeological_site", kml::BookmarkIcon::Sights},
  {"historic-monument", kml::BookmarkIcon::Sights},
  {"historic-memorial", kml::BookmarkIcon::Sights},
  {"amenity-place_of_worship", kml::BookmarkIcon::Sights},
  {"tourism-attraction", kml::BookmarkIcon::Sights},
  {"tourism-theme_park", kml::BookmarkIcon::Sights},
  {"tourism-viewpoint", kml::BookmarkIcon::Sights},
  {"historic-fort", kml::BookmarkIcon::Sights},
  {"historic-castle", kml::BookmarkIcon::Sights},
  {"tourism-artwork", kml::BookmarkIcon::Sights},
  {"historic-ruins", kml::BookmarkIcon::Sights},
  {"historic-wayside_shrine", kml::BookmarkIcon::Sights},
  {"historic-wayside_cross", kml::BookmarkIcon::Sights},

  {"tourism-gallery", kml::BookmarkIcon::Museum},
  {"tourism-museum", kml::BookmarkIcon::Museum},
  {"amenity-arts_centre", kml::BookmarkIcon::Museum},

  {"sport", kml::BookmarkIcon::Entertainment},
  {"sport-multi", kml::BookmarkIcon::Entertainment},
  {"leisure-playground", kml::BookmarkIcon::Entertainment},
  {"leisure-water_park", kml::BookmarkIcon::Entertainment},
  {"amenity-casino", kml::BookmarkIcon::Entertainment},
  {"sport-archery", kml::BookmarkIcon::Entertainment},
  {"sport-shooting", kml::BookmarkIcon::Entertainment},
  {"sport-australian_football", kml::BookmarkIcon::Entertainment},
  {"sport-bowls", kml::BookmarkIcon::Entertainment},
  {"sport-curling", kml::BookmarkIcon::Entertainment},
  {"sport-cricket", kml::BookmarkIcon::Entertainment},
  {"sport-baseball", kml::BookmarkIcon::Entertainment},
  {"sport-basketball", kml::BookmarkIcon::Entertainment},
  {"sport-american_football", kml::BookmarkIcon::Entertainment},
  {"sport-athletics", kml::BookmarkIcon::Entertainment},
  {"sport-golf", kml::BookmarkIcon::Entertainment},
  {"sport-gymnastics", kml::BookmarkIcon::Entertainment},
  {"sport-tennis", kml::BookmarkIcon::Entertainment},
  {"sport-skiing", kml::BookmarkIcon::Entertainment},
  {"sport-soccer", kml::BookmarkIcon::Entertainment},
  {"amenity-nightclub", kml::BookmarkIcon::Entertainment},
  {"amenity-cinema", kml::BookmarkIcon::Entertainment},
  {"amenity-theatre", kml::BookmarkIcon::Entertainment},
  {"leisure-stadium", kml::BookmarkIcon::Entertainment},

  {"boundary-national_park", kml::BookmarkIcon::Park},
  {"leisure-nature_reserve", kml::BookmarkIcon::Park},
  {"landuse-forest", kml::BookmarkIcon::Park},

  {"natural-beach", kml::BookmarkIcon::Swim},
  {"sport-diving", kml::BookmarkIcon::Swim},
  {"sport-scuba_diving", kml::BookmarkIcon::Swim},
  {"sport-swimming", kml::BookmarkIcon::Swim},
  {"leisure-swimming_pool", kml::BookmarkIcon::Swim},

  {"natural-cave_entrance", kml::BookmarkIcon::Mountain},
  {"natural-peak", kml::BookmarkIcon::Mountain},
  {"natural-volcano", kml::BookmarkIcon::Mountain},
  {"natural-rock", kml::BookmarkIcon::Mountain},
  {"natural-bare_rock", kml::BookmarkIcon::Mountain},

  {"amenity-veterinary", kml::BookmarkIcon::Animals},
  {"leisure-dog_park", kml::BookmarkIcon::Animals},
  {"tourism-zoo", kml::BookmarkIcon::Animals},

  {"tourism-apartment", kml::BookmarkIcon::Hotel},
  {"tourism-chalet", kml::BookmarkIcon::Hotel},
  {"tourism-guest_house", kml::BookmarkIcon::Hotel},
  {"tourism-alpine_hut", kml::BookmarkIcon::Hotel},
  {"tourism-wilderness_hut", kml::BookmarkIcon::Hotel},
  {"tourism-hostel", kml::BookmarkIcon::Hotel},
  {"tourism-motel", kml::BookmarkIcon::Hotel},
  {"tourism-resort", kml::BookmarkIcon::Hotel},
  {"tourism-hotel", kml::BookmarkIcon::Hotel},
  {"sponsored-booking", kml::BookmarkIcon::Hotel},

  {"amenity-kindergarten", kml::BookmarkIcon::Building},
  {"amenity-school", kml::BookmarkIcon::Building},
  {"office", kml::BookmarkIcon::Building},
  {"amenity-library", kml::BookmarkIcon::Building},
  {"amenity-courthouse", kml::BookmarkIcon::Building},
  {"amenity-college", kml::BookmarkIcon::Building},
  {"amenity-police", kml::BookmarkIcon::Building},
  {"amenity-prison", kml::BookmarkIcon::Building},
  {"amenity-embassy", kml::BookmarkIcon::Building},
  {"office-lawyer", kml::BookmarkIcon::Building},
  {"building-train_station", kml::BookmarkIcon::Building},
  {"building-university", kml::BookmarkIcon::Building},


  {"amenity-atm", kml::BookmarkIcon::Exchange},
  {"amenity-bureau_de_change", kml::BookmarkIcon::Exchange},
  {"amenity-bank", kml::BookmarkIcon::Exchange},

  {"amenity-vending_machine", kml::BookmarkIcon::Shop},
  {"shop", kml::BookmarkIcon::Shop},
  {"amenity-marketplace", kml::BookmarkIcon::Shop},
  {"craft", kml::BookmarkIcon::Shop},
  {"shop-pawnbroker", kml::BookmarkIcon::Shop},
  {"shop-supermarket", kml::BookmarkIcon::Shop},
  {"shop-car_repair", kml::BookmarkIcon::Shop},
  {"shop-mall", kml::BookmarkIcon::Shop},
  {"highway-services", kml::BookmarkIcon::Shop},
  {"shop-stationery", kml::BookmarkIcon::Shop},
  {"shop-variety_store", kml::BookmarkIcon::Shop},
  {"shop-alcohol", kml::BookmarkIcon::Shop},
  {"shop-wine", kml::BookmarkIcon::Shop},
  {"shop-books", kml::BookmarkIcon::Shop},
  {"shop-bookmaker", kml::BookmarkIcon::Shop},
  {"shop-bakery", kml::BookmarkIcon::Shop},
  {"shop-beauty", kml::BookmarkIcon::Shop},
  {"shop-cosmetics", kml::BookmarkIcon::Shop},
  {"shop-beverages", kml::BookmarkIcon::Shop},
  {"shop-bicycle", kml::BookmarkIcon::Shop},
  {"shop-butcher", kml::BookmarkIcon::Shop},
  {"shop-car", kml::BookmarkIcon::Shop},
  {"shop-motorcycle", kml::BookmarkIcon::Shop},
  {"shop-car_parts", kml::BookmarkIcon::Shop},
  {"shop-car_repair", kml::BookmarkIcon::Shop},
  {"shop-tyres", kml::BookmarkIcon::Shop},
  {"shop-chemist", kml::BookmarkIcon::Shop},
  {"shop-clothes", kml::BookmarkIcon::Shop},
  {"shop-computer", kml::BookmarkIcon::Shop},
  {"shop-tattoo", kml::BookmarkIcon::Shop},
  {"shop-erotic", kml::BookmarkIcon::Shop},
  {"shop-funeral_directors", kml::BookmarkIcon::Shop},
  {"shop-confectionery", kml::BookmarkIcon::Shop},
  {"shop-chocolate", kml::BookmarkIcon::Shop},
  {"amenity-ice_cream", kml::BookmarkIcon::Shop},
  {"shop-convenience", kml::BookmarkIcon::Shop},
  {"shop-copyshop", kml::BookmarkIcon::Shop},
  {"shop-photo", kml::BookmarkIcon::Shop},
  {"shop-pet", kml::BookmarkIcon::Shop},
  {"shop-department_store", kml::BookmarkIcon::Shop},
  {"shop-doityourself", kml::BookmarkIcon::Shop},
  {"shop-electronics", kml::BookmarkIcon::Shop},
  {"shop-florist", kml::BookmarkIcon::Shop},
  {"shop-furniture", kml::BookmarkIcon::Shop},
  {"shop-garden_centre", kml::BookmarkIcon::Shop},
  {"shop-gift", kml::BookmarkIcon::Shop},
  {"shop-music", kml::BookmarkIcon::Shop},
  {"shop-video", kml::BookmarkIcon::Shop},
  {"shop-musical_instrument", kml::BookmarkIcon::Shop},
  {"shop-greengrocer", kml::BookmarkIcon::Shop},
  {"shop-hairdresser", kml::BookmarkIcon::Shop},
  {"shop-hardware", kml::BookmarkIcon::Shop},
  {"shop-jewelry", kml::BookmarkIcon::Shop},
  {"shop-kiosk", kml::BookmarkIcon::Shop},
  {"shop-laundry", kml::BookmarkIcon::Shop},
  {"shop-dry_cleaning", kml::BookmarkIcon::Shop},
  {"shop-mobile_phone", kml::BookmarkIcon::Shop},
  {"shop-optician", kml::BookmarkIcon::Shop},
  {"shop-outdoor", kml::BookmarkIcon::Shop},
  {"shop-seafood", kml::BookmarkIcon::Shop},
  {"shop-shoes", kml::BookmarkIcon::Shop},
  {"shop-sports", kml::BookmarkIcon::Shop},
  {"shop-ticket", kml::BookmarkIcon::Shop},
  {"shop-toys", kml::BookmarkIcon::Shop},
  {"shop-fabric", kml::BookmarkIcon::Shop},
  {"shop-alcohol", kml::BookmarkIcon::Shop},

  {"amenity-place_of_worship-christian", kml::BookmarkIcon::Christianity},
  {"landuse-cemetery-christian", kml::BookmarkIcon::Christianity},
  {"amenity-grave_yard-christian", kml::BookmarkIcon::Christianity},

  {"amenity-place_of_worship-jewish", kml::BookmarkIcon::Judaism},

  {"amenity-place_of_worship-buddhist", kml::BookmarkIcon::Buddhism},

  {"amenity-place_of_worship-muslim", kml::BookmarkIcon::Islam},

  {"amenity-parking", kml::BookmarkIcon::Parking},
  {"vending-parking_tickets", kml::BookmarkIcon::Parking},
  {"tourism-caravan_site", kml::BookmarkIcon::Parking},
  {"amenity-bicycle_parking", kml::BookmarkIcon::Parking},
  {"amenity-taxi", kml::BookmarkIcon::Parking},
  {"amenity-car_sharing", kml::BookmarkIcon::Parking},
  {"tourism-camp_site", kml::BookmarkIcon::Parking},
  {"amenity-motorcycle_parking", kml::BookmarkIcon::Parking},

  {"amenity-fuel", kml::BookmarkIcon::Gas},
  {"amenity-charging_station", kml::BookmarkIcon::Gas},

  {"amenity-fountain", kml::BookmarkIcon::Water},
  {"natural-spring", kml::BookmarkIcon::Water},
  {"amenity-drinking_water", kml::BookmarkIcon::Water},
  {"man_made-water_tap", kml::BookmarkIcon::Water},
  {"amenity-water_point", kml::BookmarkIcon::Water},

  {"amenity-dentist", kml::BookmarkIcon::Medicine},
  {"amenity-clinic", kml::BookmarkIcon::Medicine},
  {"amenity-doctors", kml::BookmarkIcon::Medicine},
  {"amenity-hospital", kml::BookmarkIcon::Medicine},
  {"amenity-pharmacy", kml::BookmarkIcon::Medicine},
  {"amenity-clinic", kml::BookmarkIcon::Medicine},
  {"amenity-childcare", kml::BookmarkIcon::Medicine},
  {"emergency-defibrillator", kml::BookmarkIcon::Medicine}
};

void ValidateKmlData(std::unique_ptr<kml::FileData> & data)
{
  if (!data)
    return;

  for (auto & t : data->m_tracksData)
  {
    if (t.m_layers.empty())
      t.m_layers.emplace_back(kml::KmlParser::GetDefaultTrackLayer());
  }
}
}  // namespace

std::unique_ptr<kml::FileData> LoadKmlFile(std::string const & file, bool useBinary)
{
  std::unique_ptr<kml::FileData> kmlData;
  try
  {
    kmlData = LoadKmlData(FileReader(file, true /* withExceptions */), useBinary);
  }
  catch (std::exception const & e)
  {
    LOG(LWARNING, ("KML", useBinary ? "binary" : "text", "loading failure:", e.what()));
    kmlData.reset();
  }
  if (kmlData == nullptr)
    LOG(LWARNING, ("Loading bookmarks failed, file", file));
  return kmlData;
}

std::unique_ptr<kml::FileData> LoadKmlData(Reader const & reader, bool useBinary)
{
  auto data = std::make_unique<kml::FileData>();
  try
  {
    if (useBinary)
    {
      kml::binary::DeserializerKml des(*data);
      des.Deserialize(reader);
    }
    else
    {
      kml::DeserializerKml des(*data);
      des.Deserialize(reader);
    }
    ValidateKmlData(data);
  }
  catch (Reader::Exception const & e)
  {
    LOG(LWARNING, ("KML", useBinary ? "binary" : "text", "reading failure:", e.what()));
    return nullptr;
  }
  catch (kml::binary::DeserializerKml::DeserializeException const & e)
  {
    LOG(LWARNING, ("KML binary deserialization failure:", e.what()));
    return nullptr;
  }
  catch (kml::DeserializerKml::DeserializeException const & e)
  {
    LOG(LWARNING, ("KML text deserialization failure:", e.what()));
    return nullptr;
  }
  catch (std::exception const & e)
  {
    LOG(LWARNING, ("KML", useBinary ? "binary" : "text", "loading failure:", e.what()));
    return nullptr;
  }
  return data;
}

bool SaveKmlFile(kml::FileData & kmlData, std::string const & file, bool useBinary)
{
  bool success;
  try
  {
    FileWriter writer(file);
    success = SaveKmlData(kmlData, writer, useBinary);
  }
  catch (std::exception const & e)
  {
    LOG(LWARNING, ("KML", useBinary ? "binary" : "text", "saving failure:", e.what()));
    success = false;
  }
  if (!success)
    LOG(LWARNING, ("Saving bookmarks failed, file", file));
  return success;
}

bool SaveKmlData(kml::FileData & kmlData, Writer & writer, bool useBinary)
{
  try
  {
    if (useBinary)
    {
      kml::binary::SerializerKml ser(kmlData);
      ser.Serialize(writer);
    }
    else
    {
      kml::SerializerKml ser(kmlData);
      ser.Serialize(writer);
    }
  }
  catch (Writer::Exception const & e)
  {
    LOG(LWARNING, ("KML", useBinary ? "binary" : "text", "writing failure:", e.what()));
    return false;
  }
  catch (std::exception const & e)
  {
    LOG(LWARNING, ("KML", useBinary ? "binary" : "text", "serialization failure:", e.what()));
    return false;
  }
  return true;
}

void ResetIds(kml::FileData & kmlData)
{
  kmlData.m_categoryData.m_id = kml::kInvalidMarkGroupId;
  for (auto & bmData : kmlData.m_bookmarksData)
    bmData.m_id = kml::kInvalidMarkId;
  for (auto & trackData : kmlData.m_tracksData)
    trackData.m_id = kml::kInvalidTrackId;
}

void SaveFeatureTypes(feature::TypesHolder const & types, kml::BookmarkData & bmData)
{
  auto const & c = classif();
  feature::TypesHolder copy(types);
  copy.SortBySpec();
  bmData.m_featureTypes.reserve(copy.Size());
  for (auto it = copy.begin(); it != copy.end(); ++it)
  {
    bmData.m_featureTypes.push_back(c.GetIndexForType(*it));
    if (bmData.m_icon == kml::BookmarkIcon::None)
    {
      auto const typeStr = c.GetReadableObjectName(*it);
      auto const itIcon = kBookmarkTypeToIcon.find(typeStr);
      if (itIcon != kBookmarkTypeToIcon.cend())
        bmData.m_icon = itIcon->second;
    }
  }
}

std::string GetPreferredBookmarkStr(kml::LocalizableString const & name)
{
  if (name.size() == 1)
    return name.begin()->second;

  StringUtf8Multilang nameMultilang;
  for (auto const & pair : name)
    nameMultilang.AddString(pair.first, pair.second);

  auto const deviceLang = StringUtf8Multilang::GetLangIndex(languages::GetCurrentNorm());

  std::string preferredName;
  if (feature::GetPreferredName(nameMultilang, deviceLang, preferredName))
    return preferredName;

  return {};
}

std::string GetPreferredBookmarkStr(kml::LocalizableString const & name, feature::RegionData const & regionData)
{
  if (name.size() == 1)
    return name.begin()->second;

  StringUtf8Multilang nameMultilang;
  for (auto const & pair : name)
    nameMultilang.AddString(pair.first, pair.second);

  auto const deviceLang = StringUtf8Multilang::GetLangIndex(languages::GetCurrentNorm());

  std::string preferredName;
  feature::GetReadableName(regionData, nameMultilang, deviceLang, false /* allowTranslit */, preferredName);
  return preferredName;
}

std::string GetLocalizedBookmarkType(std::vector<uint32_t> const & types)
{
  if (types.empty())
    return {};

  auto const & c = classif();
  auto const type = c.GetTypeForIndex(types.front());
  CategoriesHolder const & categories = GetDefaultCategories();
  return categories.GetReadableFeatureType(type, categories.MapLocaleToInteger(languages::GetCurrentOrig()));
}

std::string GetPreferredBookmarkName(kml::BookmarkData const & bmData)
{
  std::string name = GetPreferredBookmarkStr(bmData.m_customName);
  if (name.empty())
    name = GetPreferredBookmarkStr(bmData.m_name);
  if (name.empty())
    name = GetLocalizedBookmarkType(bmData.m_featureTypes);
  return name;
}
