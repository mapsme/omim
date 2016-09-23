#include "testing/testing.hpp"

#include "editor/osm_auth.hpp"

using osm::OsmOAuth;
using osm::TKeySecret;

namespace
{
constexpr char const * kIZTestUser = "Testuser";
constexpr char const * kIZTestPassword = "testtest";
constexpr char const * kIZInvalidPassword = "123";
constexpr char const * kIZForgotPasswordEmail = "test@example.com";
constexpr char const * kFacebookToken = "EAAYYoGXMFUcBAMnNx2DoyvArn2ZBq8rvZBunMB6f7lEqpvJ7es8l6N92ZA5kZA2OElpFKgJRbQXXzZBnqqDx3s9qgc542YDR73eJjkUb8OpZC6dxZAE8cjlzLmgczyVGb0vTBURgzcrbb9Sij9K2ZBQ9bR9NnAzDC0UWrd0cof1vhrHpKbGLZCaJYMbbIJbNLg6cZD";
//constexpr char const * kGoogleToken = "eyJhbGciOiJSUzI1NiIsImtpZCI6IjVhZDc2OGE1ZDhjMTJlYmE3OGJiY2M5Yjg1ZGNlMzJhYzFjZGM3MzYifQ.eyJpc3MiOiJodHRwczovL2FjY291bnRzLmdvb2dsZS5jb20iLCJhdWQiOiI0MTgwNTMwODI0ODktaTk3MGYwbHJvYjhsNGo1aTE2a2RlOGMydnBzODN0Y2wuYXBwcy5nb29nbGV1c2VyY29udGVudC5jb20iLCJzdWIiOiIxMTQxMTI2Njc5NzEwNTQ0MTk2OTMiLCJlbWFpbF92ZXJpZmllZCI6dHJ1ZSwiYXpwIjoiNDE4MDUzMDgyNDg5LTN2a2RzZ2E3cm9pNjI4ZGh2YTZydnN0MmQ4MGY5NWZuLmFwcHMuZ29vZ2xldXNlcmNvbnRlbnQuY29tIiwiZW1haWwiOiJ6dmVyaWtAdGV4dHVhbC5ydSIsImlhdCI6MTQ1MjI2ODI0NCwiZXhwIjoxNDUyMjcxODQ0LCJuYW1lIjoiSWx5YSBadmVyZXYiLCJnaXZlbl9uYW1lIjoiSWx5YSIsImZhbWlseV9uYW1lIjoiWnZlcmV2IiwibG9jYWxlIjoicnUifQ.E55Fwdt--Jln6p-eKZS18U3KNf0233hfJtLZywOxGs9HMiZNG6xZrYwPM8OFGMhweplITtCokZR54wYDD113HH5Bmt5DbdZXgGZ8mZmqS3U_toNeHWI92Zfhew08OUDF_pR1ykV76KqjW4QGQnmeEYYs4O4I2Xw3nyUTAeTxleBHTgBNW-XZHTQc0l_gr3cWULCTuGOKGTSAO6ccVx34r8n1wfbHmWYGEtdNpJxK_AVCl64pCoXL-uEV7Cp3nSKFSW4Ei6b-DW6hygVuhMNWDUZGvxLm8CbQTOHTRRCpM5vuhcPEAQHlxZrmEpU7lLXZCDBEvM9JdDvDicg_WQNf3w";
}  // namespace

UNIT_TEST(OSM_Auth_InvalidLogin)
{
  OsmOAuth auth = OsmOAuth::IZServerAuth();
  bool result;
  TEST_NO_THROW(result = auth.AuthorizePassword(kIZTestUser, kIZInvalidPassword), ());
  TEST_EQUAL(result, false, ("invalid password"));
  TEST(!auth.IsAuthorized(), ("Should not be authorized."));
}

UNIT_TEST(OSM_Auth_Login)
{
  OsmOAuth auth = OsmOAuth::IZServerAuth();
  bool result;
  TEST_NO_THROW(result = auth.AuthorizePassword(kIZTestUser, kIZTestPassword), ());
  TEST_EQUAL(result, true, ("login to test server"));
  TEST(auth.IsAuthorized(), ("Should be authorized."));
  OsmOAuth::Response const perm = auth.Request("/permissions");
  TEST_EQUAL(perm.first, OsmOAuth::HTTP::OK, ("permission request ok"));
  TEST_NOT_EQUAL(perm.second.find("write_api"), string::npos, ("can write to api"));
}

UNIT_TEST(OSM_Auth_Facebook)
{
  OsmOAuth auth = OsmOAuth::IZServerAuth();
  bool result;
  TEST_NO_THROW(result = auth.AuthorizeFacebook(kFacebookToken), ());
  TEST_EQUAL(result, true, ("login via facebook"));
  TEST(auth.IsAuthorized(), ("Should be authorized."));
  OsmOAuth::Response const perm = auth.Request("/permissions");
  TEST_EQUAL(perm.first, OsmOAuth::HTTP::OK, ("permission with stored token request ok"));
  TEST_NOT_EQUAL(perm.second.find("write_api"), string::npos, ("can write to api"));
}

// TODO(@Zverik): Fix Google auth and uncomment test.
/*UNIT_TEST(OSM_Auth_Google)
{
  OsmOAuth auth(kConsumerKey, kConsumerSecret, kTestServer, kTestServer);
  auto result = auth.AuthorizeGoogle(kGoogleToken);
  TEST_EQUAL(result, OsmOAuth::AuthResult::OK, ("login via google"));
  TEST(auth.IsAuthorized(), ("authorized"));
  OsmOAuth::Response const perm = auth.Request("/permissions");
  TEST_EQUAL(perm.first, OsmOAuth::ResponseCode::OK, ("permission with stored token request ok"));
  TEST(perm.second.find("write_api") != string::npos, ("can write to api"));
}*/

UNIT_TEST(OSM_Auth_ForgotPassword)
{
  OsmOAuth auth = OsmOAuth::IZServerAuth();
  bool result;
  TEST_NO_THROW(result = auth.ResetPassword(kIZForgotPasswordEmail), ());
  TEST_EQUAL(result, true, ("Correct email"));
  TEST_NO_THROW(result = auth.ResetPassword("not@registered.email"), ());
  TEST_EQUAL(result, false, ("Incorrect email"));
}
