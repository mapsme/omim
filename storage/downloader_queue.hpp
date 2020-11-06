#pragma once

#include "storage/storage_defines.hpp"
#include "storage/queued_country.hpp"

namespace storage
{
class QueueInterface
{
public:
  using ForEachCountryFunction = std::function<void(QueuedCountry const & country)>;

  virtual bool IsEmpty() const = 0;
  virtual bool Contains(CountryId const & country) const = 0;
  virtual void ForEachCountry(ForEachCountryFunction const & fn) const = 0;
};
}  // namespace storage
