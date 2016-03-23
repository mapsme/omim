#include "routing/turns_notification_manager.hpp"

#include "platform/location.hpp"

namespace
{
// If the distance between two sequential turns is less than kMaxTurnDistM
// the information about the second turn will be shown or pronounced when the user is
// approaching to the first one.
double constexpr kMaxTurnDistM = 400.;

// Returns true if the closest turn is an entrance to a roundabout and the second is
// an exit form a roundabout.
// Note. There are some cases when another turn (besides an exit from roundabout)
// follows an entrance to a roundabout. It could happend in case of turns inside a roundabout.
// Returns false otherwise.
bool IsClassicEntranceToRoundabout(routing::turns::TurnItemDist const & firstTurn,
                                   routing::turns::TurnItemDist const & secondTurn)
{
  return firstTurn.m_turnItem.m_turn == routing::turns::TurnDirection::EnterRoundAbout &&
         secondTurn.m_turnItem.m_turn == routing::turns::TurnDirection::LeaveRoundAbout;
}
}  // namespace

namespace routing
{
namespace turns
{
namespace sound
{
string NotificationManager::GenerateTurnText(uint32_t distanceUnits, uint8_t exitNum,
                                             bool useThenInsteadOfDistance, TurnDirection turnDir,
                                             ::Settings::Units lengthUnits) const
{
  Notification const notification(distanceUnits, exitNum, useThenInsteadOfDistance, turnDir,
                                  lengthUnits);
  return m_getTtsText(notification);
}

void NotificationManager::GenerateTurnNotifications(vector<TurnItemDist> const & turns,
                                                    vector<string> & turnNotifications)
{
  m_secondTurnNotification = GenerateSecondTurnNotification(turns);

  turnNotifications.clear();
  if (!m_enabled || turns.empty())
    return;

  TurnItemDist const & firstTurn = turns.front();
  string firstNotification = GenerateFirstTurnSound(firstTurn.m_turnItem, firstTurn.m_distMeters);
  if (m_nextTurnNotificationProgress == PronouncedNotification::Nothing)
    return;
  if (firstNotification.empty())
    return;
  turnNotifications.emplace_back(move(firstNotification));

  // Generating notifications like "Then turn left" if necessary.
  if (turns.size() < 2)
    return;
  TurnItemDist const & secondTurn = turns[1];
  ASSERT_LESS_OR_EQUAL(firstTurn.m_distMeters, secondTurn.m_distMeters, ());
  if (secondTurn.m_distMeters - firstTurn.m_distMeters > kMaxTurnDistM &&
      !IsClassicEntranceToRoundabout(firstTurn, secondTurn))
  {
    return;
  }
  string secondNotification = GenerateTurnText(
      0 /* distanceUnits is not used because of "Then" is used */, secondTurn.m_turnItem.m_exitNum,
      true, secondTurn.m_turnItem.m_turn, m_settings.GetLengthUnits());
  if (secondNotification.empty())
    return;
  turnNotifications.emplace_back(move(secondNotification));
  // Turn notification with word "Then" (about the second turn) will be pronounced.
  // When this second turn become the first one the first notification about the turn
  // shall be skipped.
  m_turnNotificationWithThen = true;
}

string NotificationManager::GenerateFirstTurnSound(TurnItem const & turn,
                                                   double distanceToTurnMeters)
{
  if (m_nextTurnIndex != turn.m_index)
  {
    m_nextTurnNotificationProgress = PronouncedNotification::Nothing;
    m_nextTurnIndex = turn.m_index;
  }

  uint32_t const distanceToPronounceNotificationM =
      m_settings.ComputeDistToPronounceDistM(m_speedMetersPerSecond);
  if (m_nextTurnNotificationProgress == PronouncedNotification::Nothing)
  {
    if (!m_settings.TooCloseForFisrtNotification(distanceToTurnMeters))
    {
      uint32_t const startPronounceDistMeters =
          m_settings.ComputeTurnDistanceM(m_speedMetersPerSecond) +
          distanceToPronounceNotificationM;
      if (distanceToTurnMeters < startPronounceDistMeters)
      {
        if (m_turnNotificationWithThen)
        {
          FastForwardFirstTurnNotification();
        }
        else
        {
          double const distToPronounceMeters = distanceToTurnMeters - distanceToPronounceNotificationM;
          if (distToPronounceMeters < 0)
          {
            FastForwardFirstTurnNotification();
            return string();  // The current position is too close to the turn for the first
                              // notification.
          }

          // Pronouncing first turn sound notification.
          double const distToPronounceUnits =
              m_settings.ConvertMetersToUnits(distToPronounceMeters);
          uint32_t const roundedDistToPronounceUnits =
              m_settings.RoundByPresetSoundedDistancesUnits(distToPronounceUnits);
          m_nextTurnNotificationProgress = PronouncedNotification::First;
          return GenerateTurnText(roundedDistToPronounceUnits, turn.m_exitNum,
                                  false /* useThenInsteadOfDistance */, turn.m_turn,
                                  m_settings.GetLengthUnits());
        }
      }
    }
    else
    {
      // The first notification has not been pronounced but the distance to the turn is too short.
      // It happens if one turn follows shortly behind another one.
      m_nextTurnNotificationProgress = PronouncedNotification::First;
      FastForwardFirstTurnNotification();
    }
    return string();
  }

  if (m_nextTurnNotificationProgress == PronouncedNotification::First &&
      distanceToTurnMeters < distanceToPronounceNotificationM)
  {
    m_nextTurnNotificationProgress = PronouncedNotification::Second;
    FastForwardFirstTurnNotification();
    return GenerateTurnText(0 /* distanceUnits */, turn.m_exitNum,
                            false /* useThenInsteadOfDistance */, turn.m_turn,
                            m_settings.GetLengthUnits());
  }
  return string();
}

void NotificationManager::Enable(bool enable)
{
  if (enable && !m_enabled)
    Reset();
  m_enabled = enable;
}

void NotificationManager::SetLengthUnits(::Settings::Units units)
{
  m_settings.SetLengthUnits(units);
  switch (units)
  {
  case ::Settings::Metric:
    m_settings.SetState(30 /* notificationTimeSeconds */, 200 /* minNotificationDistanceUnits */,
                        2000 /* maxNotificationDistanceUnits */,
                        GetSoundedDistMeters() /* soundedDistancesUnits */,
                        ::Settings::Metric /* lengthUnits */);
    return;
  case ::Settings::Foot:
    m_settings.SetState(30 /* notificationTimeSeconds */, 500 /* minNotificationDistanceUnits */,
                        5000 /* maxNotificationDistanceUnits */,
                        GetSoundedDistFeet() /* soundedDistancesUnits */,
                        ::Settings::Foot /* lengthUnits */);
    return;
  }
}

void NotificationManager::SetSpeedMetersPerSecond(double speed)
{
  // When the quality of GPS data is bad the current speed may be less then zero.
  // It's easy to reproduce at an office with Nexus 5.
  // In that case zero meters per second is used.
  m_speedMetersPerSecond = max(0., speed);
}

void NotificationManager::Reset()
{
  m_nextTurnNotificationProgress = PronouncedNotification::Nothing;
  m_nextTurnIndex = 0;
  m_turnNotificationWithThen = false;
  m_secondTurnNotification = TurnDirection::NoTurn;
  m_secondTurnNotificationIndex = 0;
}

void NotificationManager::FastForwardFirstTurnNotification()
{
  m_turnNotificationWithThen = false;
  if (m_nextTurnNotificationProgress == PronouncedNotification::Nothing)
    m_nextTurnNotificationProgress = PronouncedNotification::First;
}

TurnDirection NotificationManager::GenerateSecondTurnNotification(vector<TurnItemDist> const & turns)
{
  if (turns.size() < 2)
  {
    m_secondTurnNotificationIndex = 0;
    return TurnDirection::NoTurn;
  }

  TurnItemDist const & firstTurn = turns[0];
  TurnItemDist const & secondTurn = turns[1];

  if (firstTurn.m_turnItem.m_index != m_secondTurnNotificationIndex)
    m_secondTurnNotificationIndex = 0;  // It's a new closest(fisrt) turn.
  else if (m_secondTurnNotificationIndex != 0)
    return secondTurn.m_turnItem.m_turn;  // m_secondTurnNotificationIndex was set to true before.

  double const distBetweenTurnsMeters = secondTurn.m_distMeters - firstTurn.m_distMeters;
  ASSERT_LESS_OR_EQUAL(0., distBetweenTurnsMeters, ());

  if (distBetweenTurnsMeters > kMaxTurnDistM)
    return TurnDirection::NoTurn;

  uint32_t const startPronounceDistMeters =
      m_settings.ComputeTurnDistanceM(m_speedMetersPerSecond) +
      m_settings.ComputeDistToPronounceDistM(m_speedMetersPerSecond);
  if (firstTurn.m_distMeters <= startPronounceDistMeters)
  {
    m_secondTurnNotificationIndex = firstTurn.m_turnItem.m_index;
    return secondTurn.m_turnItem.m_turn;  // It's time to inform about the turn after the next one.
  }
  return TurnDirection::NoTurn;
}

string DebugPrint(PronouncedNotification const notificationProgress)
{
  switch (notificationProgress)
  {
  case PronouncedNotification::Nothing:
    return "Nothing";
  case PronouncedNotification::First:
    return "First";
  case PronouncedNotification::Second:
    return "Second";
  }

  ASSERT(false, ());
  stringstream out;
  out << "unknown PronouncedNotification (" << static_cast<int>(notificationProgress) << ")";
  return out.str();
}
}  // namespace sound
}  // namespace turns
}  // namespace routing
