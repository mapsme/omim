#include "partners_api/facebook_ads.hpp"

namespace
{
#if defined(OMIM_OS_IPHONE)
  auto const kFoodPlacementId = "185237551520383_1425355070841952";
  auto const kShopsPlacementId = "185237551520383_1425355647508561";
  auto const kCityTransportPlacementId = "185237551520383_1425356124175180";
  auto const kGlobalTransportPlacementId = "185237551520383_1425356400841819";
  auto const kHotelsPlacementId = "185237551520383_1425356560841803";
  auto const kSightsPlacementId = "185237551520383_1425356720841787";
  auto const kLargeToponymsPlacementId = "185237551520383_1425356987508427";
  auto const kHealthPlacementId = "185237551520383_1425357194175073";
  auto const kFinancialPlacementId = "185237551520383_1425358064174986";
  auto const kEntertainmentPlacementId = "185237551520383_1425358274174965";
  auto const kBuildingPlacementId = "185237551520383_1425358410841618";
  auto const kBannerIdForOtherTypes = "185237551520383_1425363454174447";
#else
  auto const kFoodPlacementId = "185237551520383_1384650164912443";
  auto const kShopsPlacementId = "185237551520383_1384650804912379";
  auto const kCityTransportPlacementId = "185237551520383_1384651074912352";
  auto const kGlobalTransportPlacementId = "185237551520383_1387632484614211";
  auto const kHotelsPlacementId = "185237551520383_1384651324912327";
  auto const kSightsPlacementId = "185237551520383_1384651734912286";
  auto const kLargeToponymsPlacementId = "185237551520383_1384652164912243";
  auto const kHealthPlacementId = "185237551520383_1384652351578891";
  auto const kFinancialPlacementId = "185237551520383_1384652658245527";
  auto const kEntertainmentPlacementId = "185237551520383_1384653001578826";
  auto const kBuildingPlacementId = "185237551520383_1419317661445693";
  auto const kBannerIdForOtherTypes = "185237551520383_1384653421578784";
#endif
}  // namespace

namespace ads
{
Facebook::Facebook()
{
  AppendEntry({{"amenity", "cafe"},
               {"amenity", "fast_food"},
               {"amenity", "restaurant"},
               {"amenity", "bar"},
               {"amenity", "pub"}},
              kFoodPlacementId);

  AppendEntry({{"shop"},
               {"amenity", "marketplace"}},
              kShopsPlacementId);

  AppendEntry({{"aerialway"},
               {"highway", "bus_stop"},
               {"highway", "speed_camera"},
               {"public_transport"}},
              kCityTransportPlacementId);

  AppendEntry({{"aeroway"},
               {"railway"},
               {"man_made", "pier"}},
              kGlobalTransportPlacementId);

  AppendEntry({{"tourism", "hotel"},
               {"tourism", "hostel"},
               {"tourism", "motel"},
               {"tourism", "apartment"},
               {"tourism", "resort"}},
              kHotelsPlacementId);

  AppendEntry({{"tourism", "chalet"},
               {"tourism", "zoo"},
               {"tourism", "artwork"},
               {"tourism", "information"},
               {"tourism", "attraction"},
               {"tourism", "viewpoint"},
               {"tourism", "museum"},
               {"amenity", "fountain"},
               {"amenity", "townhall"},
               {"historic"}},
              kSightsPlacementId);

  AppendEntry({{"place"}}, kLargeToponymsPlacementId);

  AppendEntry({{"amenity", "dentist"},
               {"amenity", "doctors"},
               {"amenity", "clinic"},
               {"amenity", "hospital"},
               {"amenity", "pharmacy"},
               {"amenity", "veterinary"}},
              kHealthPlacementId);

  AppendEntry({{"amenity", "bank"}, {"amenity", "atm"}}, kFinancialPlacementId);

  AppendEntry({{"amenity", "cinema"},
               {"amenity", "brothel"},
               {"amenity", "casino"},
               {"amenity", "nightclub"},
               {"amenity", "theatre"},
               {"boundary", "national_park"},
               {"leisure"}},
              kEntertainmentPlacementId);

  AppendEntry({{"building"}}, kBuildingPlacementId);
}

std::string Facebook::GetBannerIdForOtherTypes() const
{
  return kBannerIdForOtherTypes;
}
}  // namespace ads
