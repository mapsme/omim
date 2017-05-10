#pragma once

#include "routing/routing_mapping.hpp"

#include "geometry/point2d.hpp"

#include "base/thread.hpp"

#include <string>
#include <memory>


namespace routing
{
using TCountryLocalFileFn = function<bool(std::string const &)>;

class IOnlineFetcher
{
public:
  virtual ~IOnlineFetcher() = default;
  virtual void GenerateRequest(m2::PointD const & startPoint, m2::PointD const & finalPoint) = 0;
  virtual void GetAbsentCountries(vector<std::string> & countries) = 0;
};

/*!
 * \brief The OnlineAbsentCountriesFetcher class incapsulates async fetching the map
 * names from online OSRM server routines.
 */
class OnlineAbsentCountriesFetcher : public IOnlineFetcher
{
public:
  OnlineAbsentCountriesFetcher(TCountryFileFn const & countryFileFn,
                               TCountryLocalFileFn const & countryLocalFileFn)
    : m_countryFileFn(countryFileFn), m_countryLocalFileFn(countryLocalFileFn)
  {
  }

  // IOnlineFetcher overrides:
  void GenerateRequest(m2::PointD const & startPoint, m2::PointD const & finalPoint) override;
  void GetAbsentCountries(vector<std::string> & countries) override;

private:
  TCountryFileFn const m_countryFileFn;
  TCountryLocalFileFn const m_countryLocalFileFn;
  std::unique_ptr<threads::Thread> m_fetcherThread;
};
}  // namespace routing
