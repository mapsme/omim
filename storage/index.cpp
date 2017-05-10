#include "storage/index.hpp"

#include <sstream>

namespace storage
{
storage::TCountryId const kInvalidCountryId;

bool IsCountryIdValid(TCountryId const & countryId)
{
  return countryId != kInvalidCountryId;
}
} //  namespace storage
