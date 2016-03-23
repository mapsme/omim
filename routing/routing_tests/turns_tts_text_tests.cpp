#include "testing/testing.hpp"

#include "routing/turns_sound_settings.hpp"
#include "routing/turns_tts_text.hpp"

#include "std/cstring.hpp"
#include "std/string.hpp"

namespace
{
using namespace routing::turns;
using namespace routing::turns::sound;

bool PairDistEquals(PairDist const & lhs, PairDist const & rhs)
{
  return lhs.first == rhs.first && strcmp(lhs.second, rhs.second) == 0;
}

UNIT_TEST(GetDistanceTextIdMetersTest)
{
  // Notification(uint32_t distanceUnits, uint8_t exitNum, bool useThenInsteadOfDistance,
  //    TurnDirection turnDir, ::Settings::Units lengthUnits)
  Notification const notification1(500, 0, false, TurnDirection::TurnRight, ::Settings::Metric);
  TEST_EQUAL(GetDistanceTextId(notification1), "in_500_meters", ());
  Notification const notification2(500, 0, true, TurnDirection::TurnRight, ::Settings::Metric);
  TEST_EQUAL(GetDistanceTextId(notification2), "then", ());
  Notification const notification3(200, 0, false, TurnDirection::TurnRight, ::Settings::Metric);
  TEST_EQUAL(GetDistanceTextId(notification3), "in_200_meters", ());
  Notification const notification4(2000, 0, false, TurnDirection::TurnRight, ::Settings::Metric);
  TEST_EQUAL(GetDistanceTextId(notification4), "in_2_kilometers", ());
}

UNIT_TEST(GetDistanceTextIdFeetTest)
{
  // Notification(uint32_t distanceUnits, uint8_t exitNum, bool useThenInsteadOfDistance,
  //    TurnDirection turnDir, ::Settings::Units lengthUnits)
  Notification const notification1(500, 0, false, TurnDirection::TurnRight, ::Settings::Foot);
  TEST_EQUAL(GetDistanceTextId(notification1), "in_500_feet", ());
  Notification const notification2(500, 0, true, TurnDirection::TurnRight, ::Settings::Foot);
  TEST_EQUAL(GetDistanceTextId(notification2), "then", ());
  Notification const notification3(800, 0, false, TurnDirection::TurnRight, ::Settings::Foot);
  TEST_EQUAL(GetDistanceTextId(notification3), "in_800_feet", ());
  Notification const notification4(5000, 0, false, TurnDirection::TurnRight, ::Settings::Foot);
  TEST_EQUAL(GetDistanceTextId(notification4), "in_5000_feet", ());
}

UNIT_TEST(GetRoundaboutTextIdTest)
{
  // Notification(uint32_t distanceUnits, uint8_t exitNum, bool useThenInsteadOfDistance,
  //    TurnDirection turnDir, ::Settings::Units lengthUnits)
  Notification const notification1(500, 0, false, TurnDirection::LeaveRoundAbout, ::Settings::Foot);
  TEST_EQUAL(GetRoundaboutTextId(notification1), "leave_the_roundabout", ());
  Notification const notification2(0, 3, true, TurnDirection::LeaveRoundAbout, ::Settings::Foot);
  TEST_EQUAL(GetRoundaboutTextId(notification2), "take_the_3_exit", ());
  Notification const notification3(0, 7, true, TurnDirection::LeaveRoundAbout, ::Settings::Metric);
  TEST_EQUAL(GetRoundaboutTextId(notification3), "take_the_7_exit", ());
  Notification const notification4(0, 15, true, TurnDirection::LeaveRoundAbout, ::Settings::Metric);
  TEST_EQUAL(GetRoundaboutTextId(notification4), "leave_the_roundabout", ());
}

UNIT_TEST(GetYouArriveTextIdTest)
{
  // Notification(uint32_t distanceUnits, uint8_t exitNum, bool useThenInsteadOfDistance,
  //    TurnDirection turnDir, ::Settings::Units lengthUnits)
  Notification const notification1(500, 0, false, TurnDirection::ReachedYourDestination, ::Settings::Foot);
  TEST_EQUAL(GetYouArriveTextId(notification1), "destination", ());
  Notification const notification2(0, 0, false, TurnDirection::ReachedYourDestination, ::Settings::Metric);
  TEST_EQUAL(GetYouArriveTextId(notification2), "you_have_reached_the_destination", ());
  Notification const notification3(0, 0, true, TurnDirection::ReachedYourDestination, ::Settings::Metric);
  TEST_EQUAL(GetYouArriveTextId(notification3), "destination", ());
}

UNIT_TEST(GetDirectionTextIdTest)
{
  // Notification(uint32_t distanceUnits, uint8_t exitNum, bool useThenInsteadOfDistance,
  //    TurnDirection turnDir, ::Settings::Units lengthUnits)
  Notification const notification1(500, 0, false, TurnDirection::TurnRight, ::Settings::Foot);
  TEST_EQUAL(GetDirectionTextId(notification1), "make_a_right_turn", ());
  Notification const notification2(1000, 0, false, TurnDirection::GoStraight, ::Settings::Metric);
  TEST_EQUAL(GetDirectionTextId(notification2), "go_straight", ());
  Notification const notification3(700, 0, false, TurnDirection::UTurnLeft, ::Settings::Metric);
  TEST_EQUAL(GetDirectionTextId(notification3), "make_a_u_turn", ());
  Notification const notification4(200, 0, false, TurnDirection::ReachedYourDestination, ::Settings::Metric);
  TEST_EQUAL(GetDirectionTextId(notification4), "destination", ());
  Notification const notification5(0, 0, false, TurnDirection::ReachedYourDestination, ::Settings::Metric);
  TEST_EQUAL(GetDirectionTextId(notification5), "you_have_reached_the_destination", ());
}

UNIT_TEST(GetTtsTextTest)
{
  string const engShortJson =
      "\
      {\
      \"in_300_meters\":\"In 300 meters.\",\
      \"in_500_meters\":\"In 500 meters.\",\
      \"then\":\"Then.\",\
      \"make_a_right_turn\":\"Make a right turn.\",\
      \"make_a_left_turn\":\"Make a left turn.\",\
      \"you_have_reached_the_destination\":\"You have reached the destination.\"\
      }";

  string const rusShortJson =
      "\
      {\
      \"in_300_meters\":\"Через 300 метров.\",\
      \"in_500_meters\":\"Через 500 метров.\",\
      \"then\":\"Затем.\",\
      \"make_a_right_turn\":\"Поворот направо.\",\
      \"make_a_left_turn\":\"Поворот налево.\",\
      \"you_have_reached_the_destination\":\"Вы достигли конца маршрута.\"\
      }";

  GetTtsText getTtsText;
  // Notification(uint32_t distanceUnits, uint8_t exitNum, bool useThenInsteadOfDistance,
  //    TurnDirection turnDir, Settings::Units lengthUnits)
  Notification const notification1(500, 0, false, TurnDirection::TurnRight, ::Settings::Metric);
  Notification const notification2(300, 0, false, TurnDirection::TurnLeft, ::Settings::Metric);
  Notification const notification3(0, 0, false, TurnDirection::ReachedYourDestination,
                                  ::Settings::Metric);
  Notification const notification4(0, 0, true, TurnDirection::TurnLeft, ::Settings::Metric);

  getTtsText.ForTestingSetLocaleWithJson(engShortJson, "en");
  TEST_EQUAL(getTtsText(notification1), "In 500 meters. Make a right turn.", ());
  TEST_EQUAL(getTtsText(notification2), "In 300 meters. Make a left turn.", ());
  TEST_EQUAL(getTtsText(notification3), "You have reached the destination.", ());
  TEST_EQUAL(getTtsText(notification4), "Then. Make a left turn.", ());

  getTtsText.ForTestingSetLocaleWithJson(rusShortJson, "ru");
  TEST_EQUAL(getTtsText(notification1), "Через 500 метров. Поворот направо.", ());
  TEST_EQUAL(getTtsText(notification2), "Через 300 метров. Поворот налево.", ());
  TEST_EQUAL(getTtsText(notification3), "Вы достигли конца маршрута.", ());
  TEST_EQUAL(getTtsText(notification4), "Затем. Поворот налево.", ());
}

UNIT_TEST(GetAllSoundedDistMetersTest)
{
  VecPairDist const allSoundedDistMeters = GetAllSoundedDistMeters();

  TEST(is_sorted(allSoundedDistMeters.cbegin(), allSoundedDistMeters.cend(),
                 [](PairDist const & p1, PairDist const & p2)
       {
         return p1.first < p2.first;
       }), ());

  TEST_EQUAL(allSoundedDistMeters.size(), 17, ());
  PairDist const expected1 = {50, "in_50_meters"};
  TEST(PairDistEquals(allSoundedDistMeters[0], expected1), (allSoundedDistMeters[0], expected1));
  PairDist const expected2 = {700, "in_700_meters"};
  TEST(PairDistEquals(allSoundedDistMeters[8], expected2), (allSoundedDistMeters[8], expected2));
  PairDist const expected3 = {3000, "in_3_kilometers"};
  TEST(PairDistEquals(allSoundedDistMeters[16], expected3), (allSoundedDistMeters[16], expected3));
}

UNIT_TEST(GetAllSoundedDistFeet)
{
  VecPairDist const allSoundedDistFeet = GetAllSoundedDistFeet();

  TEST(is_sorted(allSoundedDistFeet.cbegin(), allSoundedDistFeet.cend(),
                 [](PairDist const & p1, PairDist const & p2)
       {
         return p1.first < p2.first;
       }), ());

  TEST_EQUAL(allSoundedDistFeet.size(), 22, ());
  PairDist const expected1 = {50, "in_50_feet"};
  TEST(PairDistEquals(allSoundedDistFeet[0], expected1), (allSoundedDistFeet[0], expected1));
  PairDist const expected2 = {700, "in_700_feet"};
  TEST(PairDistEquals(allSoundedDistFeet[7], expected2), (allSoundedDistFeet[7], expected2));
  PairDist const expected3 = {10560, "in_2_miles"};
  TEST(PairDistEquals(allSoundedDistFeet[21], expected3), (allSoundedDistFeet[21], expected3));
}

UNIT_TEST(GetSoundedDistMeters)
{
  vector<uint32_t> const soundedDistMeters = GetSoundedDistMeters();
  VecPairDist const allSoundedDistMeters = GetAllSoundedDistMeters();

  TEST(is_sorted(soundedDistMeters.cbegin(), soundedDistMeters.cend()), ());
  // Checking that allSounded contains any element of inst.
  TEST(find_first_of(soundedDistMeters.cbegin(), soundedDistMeters.cend(),
                     allSoundedDistMeters.cbegin(), allSoundedDistMeters.cend(),
                     [](uint32_t p1, PairDist const & p2)
       {
         return p1 == p2.first;
       }) != soundedDistMeters.cend(), ());

  TEST_EQUAL(soundedDistMeters.size(), 11, ());
  TEST_EQUAL(soundedDistMeters[0], 200, ());
  TEST_EQUAL(soundedDistMeters[7], 900, ());
  TEST_EQUAL(soundedDistMeters[10], 2000, ());
}

UNIT_TEST(GetSoundedDistFeet)
{
  vector<uint32_t> soundedDistFeet = GetSoundedDistFeet();
  VecPairDist const allSoundedDistFeet = GetAllSoundedDistFeet();

  TEST(is_sorted(soundedDistFeet.cbegin(), soundedDistFeet.cend()), ());
  // Checking that allSounded contains any element of inst.
  TEST(find_first_of(soundedDistFeet.cbegin(), soundedDistFeet.cend(), allSoundedDistFeet.cbegin(),
                     allSoundedDistFeet.cend(), [](uint32_t p1, PairDist const & p2)
       {
         return p1 == p2.first;
       }) != soundedDistFeet.cend(), ());

  TEST_EQUAL(soundedDistFeet.size(), 11, ());
  TEST_EQUAL(soundedDistFeet[0], 500, ());
  TEST_EQUAL(soundedDistFeet[7], 2000, ());
  TEST_EQUAL(soundedDistFeet[10], 5000, ());
}
}  //  namespace
