#pragma once

#include <vector>

namespace pugi
{
class xml_document;
}  // namespace pugi

namespace openlr
{
struct LinearSegment;

bool ParseOpenlr(pugi::xml_document const & document, std::vector<LinearSegment> & segments);
}  // namespace openlr
