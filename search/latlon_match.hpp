#pragma once
#include <string>


namespace search
{

/// Parse input query for most input coordinates cases.
bool MatchLatLonDegree(std::string const & query, double & lat, double & lon);

}  // namespace search
