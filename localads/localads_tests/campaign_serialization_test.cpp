#include "testing/testing.hpp"

#include "localads/campaign_serialization.hpp"

#include <random>
#include <vector>


using namespace localads;

namespace
{
bool TestSerialization(std::vector<Campaign> const & cs)
{
  auto const bytes = Serialize(cs);
  LOG(LINFO, ("Src msg size:", (8+2+1) * cs.size(),
              "Comporessed size:", bytes.size()));
  return cs == Deserialize(bytes);
}

std::vector<Campaign> GenerateRandomCampaigns(size_t number)
{
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> featureIds(1, 600000);
  std::uniform_int_distribution<> icons(1, 4096);
  std::uniform_int_distribution<> expirationDays(1, 30);

  std::vector<Campaign> cs;
  while (number--)
    cs.emplace_back(featureIds(gen), icons(gen), expirationDays(gen), 0);
  return cs;
}
}  // namspace

UNIT_TEST(Serialization_Smoke)
{
  TEST(TestSerialization({
        {10, 10, 10, 0},
        {1000, 100, 20, 0},
        {120003, 456, 15, 0}
      }), ());

  TEST(TestSerialization(GenerateRandomCampaigns(100)), ());
  TEST(TestSerialization(GenerateRandomCampaigns(1000)), ());
  TEST(TestSerialization(GenerateRandomCampaigns(10000)), ());
}
