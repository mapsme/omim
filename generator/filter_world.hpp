#pragma once

#include "generator/feature_builder.hpp"
#include "generator/filter_interface.hpp"
#include "generator/popular_places_section_builder.hpp"

#include <memory>
#include <string>

namespace generator
{
class FilterWorld : public FilterInterface
{
public:
  explicit FilterWorld(std::string const & popularityFilename);

  // FilterInterface overrides:
  std::shared_ptr<FilterInterface> Clone() const override;

  bool IsAccepted(FeatureBuilder1 const & feature) override;

  static bool IsInternationalAirport(FeatureBuilder1 const & fb);
  static bool IsGoogScale(FeatureBuilder1 const & fb);
  static bool IsPopularAttraction(FeatureBuilder1 const & fb, std::string const & popularityFilename);

private:
  std::string m_popularityFilename;
};
}  // namespace generator
