#pragma once

#include "indexer/feature_data.hpp"
#include "indexer/classificator.hpp"
#include "indexer/ftypes_matcher.hpp"

#include <string>

struct MetadataTagProcessorImpl
{
  MetadataTagProcessorImpl(FeatureParams &params)
  : m_params(params)
  {
  }

  std::string ValidateAndFormat_maxspeed(std::string const & v) const;
  std::string ValidateAndFormat_stars(std::string const & v) const;
  std::string ValidateAndFormat_cuisine(std::string v) const;
  std::string ValidateAndFormat_operator(std::string const & v) const;
  std::string ValidateAndFormat_url(std::string const & v) const;
  std::string ValidateAndFormat_phone(std::string const & v) const;
  std::string ValidateAndFormat_opening_hours(std::string const & v) const;
  std::string ValidateAndFormat_ele(std::string const & v) const;
  std::string ValidateAndFormat_turn_lanes(std::string const & v) const;
  std::string ValidateAndFormat_turn_lanes_forward(std::string const & v) const;
  std::string ValidateAndFormat_turn_lanes_backward(std::string const & v) const;
  std::string ValidateAndFormat_email(std::string const & v) const;
  std::string ValidateAndFormat_postcode(std::string const & v) const;
  std::string ValidateAndFormat_flats(std::string const & v) const;
  std::string ValidateAndFormat_internet(std::string v) const;
  std::string ValidateAndFormat_height(std::string const & v) const;
  std::string ValidateAndFormat_building_levels(std::string v) const;
  std::string ValidateAndFormat_denomination(std::string const & v) const;
  std::string ValidateAndFormat_wikipedia(std::string v) const;
  std::string ValidateAndFormat_price_rate(std::string const & v) const;
  std::string ValidateAndFormat_sponsored_id(std::string const & v) const;
  std::string ValidateAndFormat_rating(std::string const & v) const;

protected:
  FeatureParams & m_params;
};

class MetadataTagProcessor : private MetadataTagProcessorImpl
{
public:
  /// Make base class constructor public.
  using MetadataTagProcessorImpl::MetadataTagProcessorImpl;
  /// Since it is used as a functor wich stops iteration in ftype::ForEachTag
  /// and the is no need for interrupting it always returns false.
  /// TODO(mgsergio): Move to cpp after merge with https://github.com/mapsme/omim/pull/1314
  bool operator() (std::string const & k, std::string const & v)
  {
    if (v.empty())
      return false;

    using feature::Metadata;
    Metadata & md = m_params.GetMetadata();

    Metadata::EType mdType;
    if (!Metadata::TypeFromString(k, mdType))
    {
      // Specific cases which do not map directly to our metadata types.
      if (k == "building:min_level")
      {
        // Converting this attribute into height only if min_height has not been already set.
        if (!md.Has(Metadata::FMD_MIN_HEIGHT))
          md.Set(Metadata::FMD_MIN_HEIGHT, ValidateAndFormat_building_levels(v));
      }
      return false;
    }

    std::string valid;
    switch (mdType)
    {
    case Metadata::FMD_CUISINE: valid = ValidateAndFormat_cuisine(v); break;
    case Metadata::FMD_OPEN_HOURS: valid = ValidateAndFormat_opening_hours(v); break;
    case Metadata::FMD_FAX_NUMBER:  // The same validator as for phone.
    case Metadata::FMD_PHONE_NUMBER: valid = ValidateAndFormat_phone(v); break;
    case Metadata::FMD_STARS: valid = ValidateAndFormat_stars(v); break;
    case Metadata::FMD_OPERATOR: valid = ValidateAndFormat_operator(v); break;
    case Metadata::FMD_URL:  // The same validator as for website.
    case Metadata::FMD_WEBSITE: valid = ValidateAndFormat_url(v); break;
    case Metadata::FMD_INTERNET: valid = ValidateAndFormat_internet(v); break;
    case Metadata::FMD_ELE: valid = ValidateAndFormat_ele(v); break;
    case Metadata::FMD_TURN_LANES: valid = ValidateAndFormat_turn_lanes(v); break;
    case Metadata::FMD_TURN_LANES_FORWARD: valid = ValidateAndFormat_turn_lanes_forward(v); break;
    case Metadata::FMD_TURN_LANES_BACKWARD: valid = ValidateAndFormat_turn_lanes_backward(v); break;
    case Metadata::FMD_EMAIL: valid = ValidateAndFormat_email(v); break;
    case Metadata::FMD_POSTCODE: valid = ValidateAndFormat_postcode(v); break;
    case Metadata::FMD_WIKIPEDIA: valid = ValidateAndFormat_wikipedia(v); break;
    case Metadata::FMD_MAXSPEED: valid = ValidateAndFormat_maxspeed(v); break;
    case Metadata::FMD_FLATS: valid = ValidateAndFormat_flats(v); break;
    case Metadata::FMD_MIN_HEIGHT:  // The same validator as for height.
    case Metadata::FMD_HEIGHT: valid = ValidateAndFormat_height(v); break;
    case Metadata::FMD_DENOMINATION: valid = ValidateAndFormat_denomination(v); break;
    case Metadata::FMD_BUILDING_LEVELS: valid = ValidateAndFormat_building_levels(v); break;
    case Metadata::FMD_SPONSORED_ID: valid = ValidateAndFormat_sponsored_id(v); break;
    case Metadata::FMD_PRICE_RATE: valid = ValidateAndFormat_price_rate(v); break;
    case Metadata::FMD_RATING: valid = ValidateAndFormat_rating(v); break;
    case Metadata::FMD_BANNER_URL: valid = ValidateAndFormat_url(v); break;

    case Metadata::FMD_TEST_ID:
    case Metadata::FMD_COUNT: CHECK(false, ("FMD_COUNT can not be used as a type."));
    }
    md.Set(mdType, valid);
    return false;
  }
};
