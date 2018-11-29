#include "map/tips_api.hpp"

#include "metrics/eye.hpp"

#include "platform/platform.hpp"

#include "base/logging.hpp"
#include "base/timer.hpp"

#include <sstream>
#include <type_traits>
#include <utility>
#include <vector>

using namespace eye;

namespace
{
// The app shouldn't show any screen at all more frequently than once in 3 days.
auto constexpr kShowAnyTipPeriod = std::chrono::hours(24) * 3;
// The app shouldn't show the same screen more frequently than 1 month.
auto constexpr kShowSameTipPeriod = std::chrono::hours(24) * 30;
// Every current trigger for a tips screen can be activated at the start of the application by
// default and if the app was inactive more then 12 hours.
auto constexpr kShowTipAfterCollapsingPeriod = std::chrono::seconds(12 * 60 * 60);
// If a user clicks on the action areas (highlighted or blue button)
// the appropriate screen will never be shown again.
size_t constexpr kActionClicksCountToDisable = 1;
// If a user clicks 3 times on the button GOT IT the appropriate screen will never be shown again.
size_t constexpr kGotitClicksCountToDisable = 3;

template <typename T, std::enable_if_t<std::is_enum<T>::value> * = nullptr>
size_t ToIndex(T type)
{
  return static_cast<size_t>(type);
}

boost::optional<eye::Tip::Type> GetTipImpl(TipsApi::Duration showAnyTipPeriod,
                                           TipsApi::Duration showSameTipPeriod,
                                           TipsApi::Delegate const & delegate,
                                           TipsApi::Conditions const & conditions)
{
  auto const lastBackgroundTime = delegate.GetLastBackgroundTime();
  if (lastBackgroundTime != 0.0)
  {
    auto const timeInBackground =
        static_cast<uint64_t>(base::Timer::LocalTime() - lastBackgroundTime);
    if (timeInBackground < kShowTipAfterCollapsingPeriod.count())
      return {};
  }

  auto const info = Eye::Instance().GetInfo();

  CHECK(info, ("Eye info must be initialized"));
  LOG(LINFO, ("Eye info ptr use count:", info.use_count(), "Info", *info, "Info::m_booking ref:",
    &(info->m_booking), "Info::m_bookmarks ref:", &(info->m_bookmarks), "Info::m_discovery ref:",
    &(info->m_discovery), "Info::m_layers ref:", &(info->m_layers), "Layers size:",
    info->m_layers.size(), "Info::m_tips ref:", &(info->m_tips), "Tips size:", info->m_tips.size(),
    "Conditions ref:", &conditions, "Delegate ref:", &delegate));

  auto const & tips = info->m_tips;
  auto constexpr totalTipsCount = static_cast<size_t>(Tip::Type::Count);

  if (!tips.empty())
    LOG(LINFO, ("Tips data ptr:", tips.data()));

  CHECK_EQUAL(conditions.size(), totalTipsCount, ());

  LOG(LINFO, ("Conditions data ptr:", conditions.data()));

  for (size_t i = 0; i < conditions.size(); ++i)
  {
    CHECK(conditions[i] != nullptr, ("Condition at index", i, "is not initialized"));
  }

  Time lastShownTime;
  for (auto const & tip : tips)
  {
    if (lastShownTime < tip.m_lastShownTime)
      lastShownTime = tip.m_lastShownTime;
  }

  if (Clock::now() - lastShownTime <= showAnyTipPeriod)
    return {};

  LOG(LINFO, ("Tip showing is allowed by time interval"));

  // If some tips are never shown.
  if (tips.size() < totalTipsCount)
  {
    LOG(LINFO, ("Try to show candidate witch was not shown."));
    using Candidate = std::pair<Tip::Type, bool>;
    std::array<Candidate, totalTipsCount> candidates;
    for (size_t i = 0; i < totalTipsCount; ++i)
    {
      candidates[i] = {static_cast<Tip::Type>(i), true};
    }

    for (auto const & shownTip : tips)
    {
      candidates[ToIndex(shownTip.m_type)].second = false;
    }

    LOG(LINFO, ("Tips candidates are filled"));

    for (auto const & c : candidates)
    {
      LOG(LINFO, ("Try to use candidate", c.first));

      if (c.second && conditions[ToIndex(c.first)](*info))
      {
        {
          std::ostringstream os;
          os << "Condition for tip " << DebugPrint(c.first)
             << " returned true. Previously shown tips: [ ";
          for (auto const & candidate : candidates)
          {
            if (!candidate.second)
              os << DebugPrint(candidate.first) << " ";
          }
          os << "]";
          LOG(LINFO, (os.str()));
        }

        return c.first;
      }
    }
  }

  LOG(LINFO, ("Try to show candidate witch was already shown."));

  for (auto const & shownTip : tips)
  {
    LOG(LINFO, ("Try to use candidate", shownTip));

    if (shownTip.m_eventCounters.Get(Tip::Event::ActionClicked) < kActionClicksCountToDisable &&
        shownTip.m_eventCounters.Get(Tip::Event::GotitClicked) < kGotitClicksCountToDisable &&
        Clock::now() - shownTip.m_lastShownTime > showSameTipPeriod &&
        conditions[ToIndex(shownTip.m_type)](*info))
    {
      LOG(LINFO, ("Condition for tip", shownTip.m_type, "returned true"));
      return shownTip.m_type;
    }
  }

  LOG(LINFO, ("No tips found"));
  return {};
}
}  // namespace

// static
TipsApi::Duration TipsApi::GetShowAnyTipPeriod()
{
  return kShowAnyTipPeriod;
}

// static
TipsApi::Duration TipsApi::GetShowSameTipPeriod()
{
  return kShowSameTipPeriod;
}

// static
TipsApi::Duration TipsApi::ShowTipAfterCollapsingPeriod()
{
  return kShowTipAfterCollapsingPeriod;
};

// static
size_t TipsApi::GetActionClicksCountToDisable()
{
 return kActionClicksCountToDisable;
}

// static
size_t TipsApi::GetGotitClicksCountToDisable()
{
  return kGotitClicksCountToDisable;
}

TipsApi::TipsApi(Delegate const & delegate)
  : m_delegate(delegate)
{
  // Condition for Tips::Type::BookmarksCatalog type.
  m_conditions.emplace_back(
  [] (eye::Info const & info)
  {
    LOG(LINFO, ("Check condition for BookmarksCatalog tip"));
    auto const result = info.m_bookmarks.m_lastOpenedTime.time_since_epoch().count() == 0 &&
                 GetPlatform().ConnectionStatus() != Platform::EConnectionType::CONNECTION_NONE;
    LOG(LINFO, ("Result for BookmarksCatalog tip condition is", result));
    return result;
  });

  // Condition for Tips::Type::BookingHotels type.
  m_conditions.emplace_back(
  [] (eye::Info const & info)
  {
    LOG(LINFO, ("Check condition for BookingHotels tip"));
    auto const result = info.m_booking.m_lastFilterUsedTime.time_since_epoch().count() == 0;
    LOG(LINFO, ("Result for BookingHotels tip condition is", result));
    return result;
  });

  // Condition for Tips::Type::DiscoverButton type.
  m_conditions.emplace_back(
  [this] (eye::Info const & info)
  {
    LOG(LINFO, ("Check condition for DiscoverButton tip"));
    auto const eventsCount = ToIndex(Discovery::Event::Count);
    for (size_t i = 0; i < eventsCount; ++i)
    {
      if (info.m_discovery.m_eventCounters.Get(static_cast<Discovery::Event>(i)) != 0)
      {
        LOG(LINFO, ("Discovery was used, false will be returned"));
        return false;
      }
    }

    LOG(LINFO, ("Get current position will be called. this ptr:", this, "delegate ref:", &m_delegate));
    auto const pos = m_delegate.GetCurrentPosition();
    if (!pos.is_initialized())
    {
      LOG(LINFO, ("Current position is not found, false will be returned"));
      return false;
    }

    LOG(LINFO, ("Is country loaded will be called. this ptr:", this, "delegate ref:", &m_delegate));
    auto const result = m_delegate.IsCountryLoaded(pos.get());

    LOG(LINFO, ("Is country loaded retured", result));
    return result;
  });

  // Condition for Tips::Type::PublicTransport type.
  m_conditions.emplace_back(
  [this] (eye::Info const & info)
  {
    LOG(LINFO, ("Check condition for PublicTransport tip. Used layers size:", info.m_layers.size()));
    if (!info.m_layers.empty())
      LOG(LINFO, ("Layers data ptr:", info.m_layers.data()));

    for (auto const & layer : info.m_layers)
    {
      if (layer.m_type == Layer::Type::PublicTransport &&
          layer.m_lastTimeUsed.time_since_epoch().count() != 0)
      {
        LOG(LINFO, ("PublicTransport was used, false will be returned"));
        return false;
      }
    }

    LOG(LINFO, ("Get current position will be called. this ptr:", this, "delegate ref:", &m_delegate));
    auto const pos = m_delegate.GetCurrentPosition();
    if (!pos.is_initialized())
    {
      LOG(LINFO, ("Current position is not found, false will be returned"));
      return false;
    }

    LOG(LINFO, ("Have transit will be called. this ptr:", this, "delegate ref:", &m_delegate));
    auto const result = m_delegate.HaveTransit(pos.get());

    LOG(LINFO, ("Have transit retured", result));
    return result;
  });
}

boost::optional<eye::Tip::Type> TipsApi::GetTip() const
{
  auto coinToss = std::time(nullptr) % 50;
  // Try to show tip in 2% of cases.
  if (coinToss != 0)
    return {};

  return GetTipImpl(GetShowAnyTipPeriod(), GetShowSameTipPeriod(), m_delegate, m_conditions);
}

// static
boost::optional<eye::Tip::Type> TipsApi::GetTipForTesting(Duration showAnyTipPeriod,
                                                          Duration showSameTipPeriod,
                                                          TipsApi::Delegate const & delegate,
                                                          Conditions const & triggers)
{
  return GetTipImpl(showAnyTipPeriod, showSameTipPeriod, delegate, triggers);
}
