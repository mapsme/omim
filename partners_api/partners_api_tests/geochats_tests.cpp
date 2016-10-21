#include "testing/testing.hpp"

#include "partners_api/geochats_api.hpp"

#include "geometry/latlon.hpp"

#include "3party/jansson/myjansson.hpp"

namespace
{
bool IsComplete(geochats::ChatInfo const & info)
{
  return !info.m_id.empty() && !info.m_name.empty() && info.m_membersCount > 0;
}
}  // namespace

UNIT_TEST(Geochats_GetGeochats)
{
  auto const rawResponse = geochats::RawApi::GetGeochats(ms::LatLon(55.797061, 37.537810), 2000);

  TEST(!rawResponse.empty(), ());
  my::Json answer(rawResponse.c_str());

  auto const status = json_object_get(answer.get(), "status");

  TEST(status != nullptr, ("Status field is not found"));

  json_int_t code;
  TEST_NO_THROW(my::FromJSONObject(status, "code", code), ());
  auto errorCode = strings::to_string(code);

  TEST(!errorCode.empty(), ());
  TEST_EQUAL(errorCode.front(), '2', ());

  auto const results = json_object_get(answer.get(), "results");

  TEST(results != nullptr, ("Results section is not found"));

  auto const chats = json_object_get(results, "chats");
  TEST(results != nullptr, ("Chats section is not found"));
  TEST(json_is_array(chats), ("Chats section is not an array"));
}

UNIT_TEST(Geochats_Smoke)
{
  vector<geochats::ChatInfo> returnedChats;
  uint64_t returnedId = 0;
  geochats::Api geochatsApi;

  auto const id = geochatsApi.GetChats(ms::LatLon(55.797061, 37.537810), 2000,
      [&returnedChats, &returnedId](vector<geochats::ChatInfo> const & chats, uint64_t const requestId) {
        returnedChats = chats;
        returnedId = requestId;

        testing::StopEventLoop();
      });

  testing::RunEventLoop();

  TEST_EQUAL(id, returnedId, ());
  TEST(!returnedChats.empty(), ());

  for (auto const & chat : returnedChats)
    TEST(IsComplete(chat), ());
}
