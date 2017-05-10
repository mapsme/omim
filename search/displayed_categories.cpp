#include "search/displayed_categories.hpp"

namespace
{
std::vector<std::string> const kKeys = {"food",     "hotel",  "tourism", "wifi", "transport",     "fuel",
                              "parking",  "shop",   "atm",     "bank", "entertainment", "hospital",
                              "pharmacy", "police", "toilet",  "post"};
}  // namespace

namespace search
{
DisplayedCategories::DisplayedCategories(CategoriesHolder const & holder) : m_holder(holder) {}

// static
std::vector<std::string> const & DisplayedCategories::GetKeys() { return kKeys; }
}  // namespace search
