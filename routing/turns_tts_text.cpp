#include "routing/turns_sound_settings.hpp"
#include "routing/turns_tts_text.hpp"

#include "base/string_utils.hpp"

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>

namespace
{
using namespace routing::turns::sound;

template <class TIter> std::string DistToTextId(TIter begin, TIter end, uint32_t dist)
{
  using TValue = typename std::iterator_traits<TIter>::value_type;

  TIter distToSound = std::lower_bound(begin, end, dist, [](TValue const & p1, uint32_t p2)
                      {
                        return p1.first < p2;
                      });
  if (distToSound == end)
  {
    ASSERT(false, ("notification.m_distanceUnits is not correct."));
    return std::string();
  }
  return distToSound->second;
}
}  //  namespace

namespace routing
{
namespace turns
{
namespace sound
{
void GetTtsText::SetLocale(std::string const & locale)
{
  m_getCurLang = platform::GetTextByIdFactory(platform::TextSource::TtsSound, locale);
}

void GetTtsText::ForTestingSetLocaleWithJson(std::string const & jsonBuffer, std::string const & locale)
{
  m_getCurLang = platform::ForTestingGetTextByIdFactory(jsonBuffer, locale);
}

std::string GetTtsText::operator()(Notification const & notification) const
{
  if (notification.m_distanceUnits == 0 && !notification.m_useThenInsteadOfDistance)
    return GetTextById(GetDirectionTextId(notification));
  if (notification.m_useThenInsteadOfDistance && notification.m_turnDir == TurnDirection::NoTurn)
    return std::string();

  std::string const dirStr = GetTextById(GetDirectionTextId(notification));
  if (dirStr.empty())
    return std::string();

  std::string const distStr = GetTextById(GetDistanceTextId(notification));
  return distStr + " " + dirStr;
}

std::string GetTtsText::GetLocale() const
{
  if (m_getCurLang == nullptr)
  {
    ASSERT(false, ());
    return std::string();
  }
  return m_getCurLang->GetLocale();
}

std::string GetTtsText::GetTextById(std::string const & textId) const
{
  ASSERT(!textId.empty(), ());

  if (m_getCurLang == nullptr)
  {
    ASSERT(false, ());
    return std::string();
  }
  return (*m_getCurLang)(textId);
}

std::string GetDistanceTextId(Notification const & notification)
{
  if (notification.m_useThenInsteadOfDistance)
    return "then";

  switch (notification.m_lengthUnits)
  {
  case measurement_utils::Units::Metric:
    return DistToTextId(GetAllSoundedDistMeters().cbegin(), GetAllSoundedDistMeters().cend(),
                        notification.m_distanceUnits);
  case measurement_utils::Units::Imperial:
    return DistToTextId(GetAllSoundedDistFeet().cbegin(), GetAllSoundedDistFeet().cend(),
                        notification.m_distanceUnits);
  }
  ASSERT(false, ());
  return std::string();
}

std::string GetRoundaboutTextId(Notification const & notification)
{
  if (notification.m_turnDir != TurnDirection::LeaveRoundAbout)
  {
    ASSERT(false, ());
    return std::string();
  }
  if (!notification.m_useThenInsteadOfDistance)
    return "leave_the_roundabout"; // Notification just before leaving a roundabout.

  static const uint8_t kMaxSoundedExit = 11;
  if (notification.m_exitNum == 0 || notification.m_exitNum > kMaxSoundedExit)
    return "leave_the_roundabout";

  return "take_the_" + strings::to_string(static_cast<int>(notification.m_exitNum)) + "_exit";
}

std::string GetYouArriveTextId(Notification const & notification)
{
  if (notification.m_turnDir != TurnDirection::ReachedYourDestination)
  {
    ASSERT(false, ());
    return std::string();
  }

  if (notification.m_distanceUnits != 0 || notification.m_useThenInsteadOfDistance)
    return "destination";
  return "you_have_reached_the_destination";
}

std::string GetDirectionTextId(Notification const & notification)
{
  switch (notification.m_turnDir)
  {
    case TurnDirection::GoStraight:
      return "go_straight";
    case TurnDirection::TurnRight:
      return "make_a_right_turn";
    case TurnDirection::TurnSharpRight:
      return "make_a_sharp_right_turn";
    case TurnDirection::TurnSlightRight:
      return "make_a_slight_right_turn";
    case TurnDirection::TurnLeft:
      return "make_a_left_turn";
    case TurnDirection::TurnSharpLeft:
      return "make_a_sharp_left_turn";
    case TurnDirection::TurnSlightLeft:
      return "make_a_slight_left_turn";
    case TurnDirection::UTurnLeft:
    case TurnDirection::UTurnRight:
      return "make_a_u_turn";
    case TurnDirection::EnterRoundAbout:
      return "enter_the_roundabout";
    case TurnDirection::LeaveRoundAbout:
      return GetRoundaboutTextId(notification);
    case TurnDirection::ReachedYourDestination:
      return GetYouArriveTextId(notification);
    case TurnDirection::StayOnRoundAbout:
    case TurnDirection::StartAtEndOfStreet:
    case TurnDirection::TakeTheExit:
    case TurnDirection::NoTurn:
    case TurnDirection::Count:
      ASSERT(false, ());
      return std::string();
  }
  ASSERT(false, ());
  return std::string();
}
}  // namespace sound
}  // namespace turns
}  // namespace routing
