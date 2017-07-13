#include "testing/testing.hpp"

#include "ugc/api.hpp"
#include "ugc/binary/serdes.hpp"
#include "ugc/binary/ugc_holder.hpp"
#include "ugc/types.hpp"

#include <cstdint>
#include <vector>

using namespace std;
using namespace ugc::binary;
using namespace ugc;

namespace
{
vector<TranslationKey> const GetExpectedTranslationKeys()
{
  vector<TranslationKey> keys = {
      {"food", "service", "music", "best-drink", "best-meal", "Coffee", "Cherry Pie"}};
  sort(keys.begin(), keys.end());
  return keys;
}

void CollectTexts(UGC const & ugc, vector<Text> & texts)
{
  for (auto const & review : ugc.m_reviews)
    texts.emplace_back(review.m_text);
}

void CollectTexts(vector<IndexUGC> const & ugcs, vector<vector<Text>> & texts)
{
  for (auto const & ugc : ugcs)
  {
    texts.emplace_back();
    CollectTexts(ugc.m_ugc, texts.back());
  }
}

UNIT_TEST(TranslationKeys_Smoke)
{
  UGCHolder holder;
  holder.Add(0 /* index */, Api::MakeTestUGC1());
  holder.Add(1 /* index */, Api::MakeTestUGC2());

  vector<vector<Text>> texts;
  CollectTexts(holder.m_ugcs, texts);

  UGCSeriaizer serializer(move(holder.m_ugcs));

  TEST_EQUAL(GetExpectedTranslationKeys(), serializer.GetTranslationKeys(), ());
  TEST_EQUAL(texts, serializer.GetTexts(), ());
}

UNIT_TEST(BinarySerDes_Smoke)
{
  vector<uint8_t> buffer;

  using Sink = MemWriter<decltype(buffer)>;

  auto const expectedUGC1 = Api::MakeTestUGC1(Time(chrono::hours(24 * 123)));
  auto const expectedUGC2 = Api::MakeTestUGC2(Time(chrono::hours(24 * 456)));

  {
    UGCHolder holder;
    holder.Add(31337 /* index */, expectedUGC1);
    holder.Add(12345 /* index */, expectedUGC2);

    Sink sink(buffer);
    UGCSeriaizer ser(move(holder.m_ugcs));
    TEST_EQUAL(GetExpectedTranslationKeys(), ser.GetTranslationKeys(), ());
    ser.Serialize(sink);
  }

  UGCDeserializerV0 des;

  {
    MemReader reader(buffer.data(), buffer.size());

    TEST(des.GetTranslationKeys().empty(), ());
    des.InitializeIfNeeded(reader);
    TEST_EQUAL(GetExpectedTranslationKeys(), des.GetTranslationKeys(), ());

    UGC ugc;
    TEST(!des.Deserialize(reader, 0 /* index */, ugc), ());

    TEST(des.Deserialize(reader, 12345 /* index */, ugc), ());
    TEST_EQUAL(ugc, expectedUGC2, ());

    TEST(des.Deserialize(reader, 31337 /* index */, ugc), ());
    TEST_EQUAL(ugc, expectedUGC1, ());
  }

  {
    MemReader reader(buffer.data(), buffer.size());

    UGC ugc;
    TEST(des.Deserialize(reader, 31337 /* index */, ugc), ());
    TEST_EQUAL(ugc, expectedUGC1, ());

    TEST(des.Deserialize(reader, 12345 /* index */, ugc), ());
    TEST_EQUAL(ugc, expectedUGC2, ());
  }
}
}  // namespace
