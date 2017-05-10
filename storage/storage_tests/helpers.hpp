#pragma once

#include "geometry/mercator.hpp"

#include <memory>

namespace storage
{
class CountryInfoGetter;

std::unique_ptr<CountryInfoGetter> CreateCountryInfoGetter();
std::unique_ptr<CountryInfoGetter> CreateCountryInfoGetterMigrate();

bool AlmostEqualRectsAbs(const m2::RectD & r1, const m2::RectD & r2);
} // namespace storage
