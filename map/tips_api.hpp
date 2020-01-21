#pragma once

#include "storage/storage_defines.hpp"

#include "metrics/eye_info.hpp"

#include "geometry/point2d.hpp"

#include <array>
#include <cstdint>
#include <chrono>
#include <functional>
#include <memory>

#include <boost/optional.hpp>

class TipsApi
{
public:
  using Duration = std::chrono::hours;
  using Condition = std::function<bool(eye::Info const & info)>;
  using Conditions = std::array<Condition, static_cast<size_t>(eye::Tip::Type::Count)>;

  class Delegate
  {
  public:
    virtual ~Delegate() = default;

    virtual boost::optional<m2::PointD> GetCurrentPosition() const = 0;
    virtual bool IsCountryLoaded(m2::PointD const & pt) const = 0;
    virtual bool HaveTransit(m2::PointD const & pt) const = 0;
    virtual double GetLastBackgroundTime() const = 0;
    virtual m2::PointD const & GetViewportCenter() const = 0;
    virtual storage::CountryId GetCountryId(m2::PointD const & pt) const = 0;
    virtual int64_t GetCountryVersion(storage::CountryId const & countryId) const = 0;
  };

  static Duration GetShowAnyTipPeriod();
  static Duration GetShowSameTipPeriod();
  static Duration ShowTipAfterCollapsingPeriod();
  static size_t GetActionClicksCountToDisable();
  static size_t GetGotitClicksCountToDisable();

  explicit TipsApi(std::unique_ptr<Delegate> delegate);

  boost::optional<eye::Tip::Type> GetTip() const;

  static boost::optional<eye::Tip::Type> GetTipForTesting(Duration showAnyTipPeriod,
                                                          Duration showSameTipPeriod,
                                                          TipsApi::Delegate const & delegate,
                                                          Conditions const & triggers);

private:
  std::unique_ptr<Delegate> m_delegate;
  Conditions m_conditions;
};
