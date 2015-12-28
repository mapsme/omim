#include "testing/testing.hpp"

#include "coding/compressed_bit_vector.hpp"
#include "coding/writer.hpp"

#include "std/algorithm.hpp"
#include "std/iterator.hpp"
#include "std/set.hpp"

namespace
{
void Intersect(vector<uint64_t> & setBits1, vector<uint64_t> & setBits2, vector<uint64_t> & result)
{
  sort(setBits1.begin(), setBits1.end());
  sort(setBits2.begin(), setBits2.end());
  set_intersection(setBits1.begin(), setBits1.end(), setBits2.begin(), setBits2.end(),
                   back_inserter(result));
}

void Subtract(vector<uint64_t> & setBits1, vector<uint64_t> & setBits2, vector<uint64_t> & result)
{
  sort(setBits1.begin(), setBits1.end());
  sort(setBits2.begin(), setBits2.end());
  set_difference(setBits1.begin(), setBits1.end(), setBits2.begin(), setBits2.end(),
                 back_inserter(result));
}

template <typename TBinaryOp>
void CheckBinaryOp(TBinaryOp op, vector<uint64_t> & setBits1, vector<uint64_t> & setBits2,
                   coding::CompressedBitVector const & cbv)
{
  vector<uint64_t> expected;
  op(setBits1, setBits2, expected);
  TEST_EQUAL(expected.size(), cbv.PopCount(), ());

  vector<bool> expectedBitmap;
  if (!expected.empty())
    expectedBitmap.resize(expected.back() + 1);

  for (size_t i = 0; i < expected.size(); ++i)
    expectedBitmap[expected[i]] = true;
  for (size_t i = 0; i < expectedBitmap.size(); ++i)
    TEST_EQUAL(cbv.GetBit(i), expectedBitmap[i], ());
}

void CheckIntersection(vector<uint64_t> & setBits1, vector<uint64_t> & setBits2,
                       coding::CompressedBitVector const & cbv)
{
  CheckBinaryOp(&Intersect, setBits1, setBits2, cbv);
}

void CheckSubtraction(vector<uint64_t> & setBits1, vector<uint64_t> & setBits2,
                      coding::CompressedBitVector const & cbv)
{
  CheckBinaryOp(&Subtract, setBits1, setBits2, cbv);
}
}  // namespace

UNIT_TEST(CompressedBitVector_Intersect1)
{
  size_t const kNumBits = 100;
  vector<uint64_t> setBits1;
  vector<uint64_t> setBits2;
  for (size_t i = 0; i < kNumBits; ++i)
  {
    if (i > 0)
      setBits1.push_back(i);
    if (i + 1 < kNumBits)
      setBits2.push_back(i);
  }
  auto cbv1 = coding::CompressedBitVectorBuilder::FromBitPositions(setBits1);
  auto cbv2 = coding::CompressedBitVectorBuilder::FromBitPositions(setBits2);
  TEST(cbv1.get(), ());
  TEST(cbv2.get(), ());

  auto cbv3 = coding::CompressedBitVector::Intersect(*cbv1, *cbv2);
  TEST_EQUAL(coding::CompressedBitVector::StorageStrategy::Dense, cbv3->GetStorageStrategy(), ());
  CheckIntersection(setBits1, setBits2, *cbv3);
}

UNIT_TEST(CompressedBitVector_Intersect2)
{
  size_t const kNumBits = 100;
  vector<uint64_t> setBits1;
  vector<uint64_t> setBits2;
  for (size_t i = 0; i < kNumBits; ++i)
  {
    if (i <= kNumBits / 2)
      setBits1.push_back(i);
    if (i >= kNumBits / 2)
      setBits2.push_back(i);
  }
  auto cbv1 = coding::CompressedBitVectorBuilder::FromBitPositions(setBits1);
  auto cbv2 = coding::CompressedBitVectorBuilder::FromBitPositions(setBits2);
  TEST(cbv1.get(), ());
  TEST(cbv2.get(), ());

  auto cbv3 = coding::CompressedBitVector::Intersect(*cbv1, *cbv2);
  TEST_EQUAL(coding::CompressedBitVector::StorageStrategy::Sparse, cbv3->GetStorageStrategy(), ());
  CheckIntersection(setBits1, setBits2, *cbv3);
}

UNIT_TEST(CompressedBitVector_Intersect3)
{
  size_t const kNumBits = 100;
  vector<uint64_t> setBits1;
  vector<uint64_t> setBits2;
  for (size_t i = 0; i < kNumBits; ++i)
  {
    if (i % 2 == 0)
      setBits1.push_back(i);
    if (i % 3 == 0)
      setBits2.push_back(i);
  }
  auto cbv1 = coding::CompressedBitVectorBuilder::FromBitPositions(setBits1);
  auto cbv2 = coding::CompressedBitVectorBuilder::FromBitPositions(setBits2);
  TEST(cbv1.get(), ());
  TEST(cbv2.get(), ());
  auto cbv3 = coding::CompressedBitVector::Intersect(*cbv1, *cbv2);
  TEST_EQUAL(coding::CompressedBitVector::StorageStrategy::Sparse, cbv3->GetStorageStrategy(), ());
  CheckIntersection(setBits1, setBits2, *cbv3);
}

UNIT_TEST(CompressedBitVector_Intersect4)
{
  size_t const kNumBits = 1000;
  vector<uint64_t> setBits1;
  vector<uint64_t> setBits2;
  for (size_t i = 0; i < kNumBits; ++i)
  {
    if (i % 100 == 0)
      setBits1.push_back(i);
    if (i % 150 == 0)
      setBits2.push_back(i);
  }
  auto cbv1 = coding::CompressedBitVectorBuilder::FromBitPositions(setBits1);
  auto cbv2 = coding::CompressedBitVectorBuilder::FromBitPositions(setBits2);
  TEST(cbv1.get(), ());
  TEST(cbv2.get(), ());
  auto cbv3 = coding::CompressedBitVector::Intersect(*cbv1, *cbv2);
  TEST_EQUAL(coding::CompressedBitVector::StorageStrategy::Sparse, cbv3->GetStorageStrategy(), ());
  CheckIntersection(setBits1, setBits2, *cbv3);
}

UNIT_TEST(CompressedBitVector_Subtract1)
{
  vector<uint64_t> setBits1 = {0, 1, 2, 3, 4, 5, 6};
  vector<uint64_t> setBits2 = {1, 2, 3, 4, 5, 6, 7};

  auto cbv1 = coding::CompressedBitVectorBuilder::FromBitPositions(setBits1);
  auto cbv2 = coding::CompressedBitVectorBuilder::FromBitPositions(setBits2);
  TEST(cbv1.get(), ());
  TEST(cbv2.get(), ());
  TEST_EQUAL(coding::CompressedBitVector::StorageStrategy::Dense, cbv1->GetStorageStrategy(), ());
  TEST_EQUAL(coding::CompressedBitVector::StorageStrategy::Dense, cbv2->GetStorageStrategy(), ());

  auto cbv3 = coding::CompressedBitVector::Subtract(*cbv1, *cbv2);
  TEST(cbv3.get(), ());
  TEST_EQUAL(coding::CompressedBitVector::StorageStrategy::Dense, cbv3->GetStorageStrategy(), ());
  CheckSubtraction(setBits1, setBits2, *cbv3);
}

UNIT_TEST(CompressedBitVector_Subtract2)
{
  vector<uint64_t> setBits1;
  for (size_t i = 0; i < 100; ++i)
    setBits1.push_back(i);

  vector<uint64_t> setBits2 = {9, 14};
  auto cbv1 = coding::CompressedBitVectorBuilder::FromBitPositions(setBits1);
  auto cbv2 = coding::CompressedBitVectorBuilder::FromBitPositions(setBits2);
  TEST(cbv1.get(), ());
  TEST(cbv2.get(), ());
  TEST_EQUAL(coding::CompressedBitVector::StorageStrategy::Dense, cbv1->GetStorageStrategy(), ());
  TEST_EQUAL(coding::CompressedBitVector::StorageStrategy::Sparse, cbv2->GetStorageStrategy(), ());

  auto cbv3 = coding::CompressedBitVector::Subtract(*cbv1, *cbv2);
  TEST(cbv3.get(), ());
  TEST_EQUAL(coding::CompressedBitVector::StorageStrategy::Dense, cbv3->GetStorageStrategy(), ());
  CheckSubtraction(setBits1, setBits2, *cbv3);
}

UNIT_TEST(CompressedBitVector_Subtract3)
{
  vector<uint64_t> setBits1 = {0, 9};
  vector<uint64_t> setBits2 = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  auto cbv1 = coding::CompressedBitVectorBuilder::FromBitPositions(setBits1);
  auto cbv2 = coding::CompressedBitVectorBuilder::FromBitPositions(setBits2);
  TEST(cbv1.get(), ());
  TEST(cbv2.get(), ());
  TEST_EQUAL(coding::CompressedBitVector::StorageStrategy::Sparse, cbv1->GetStorageStrategy(), ());
  TEST_EQUAL(coding::CompressedBitVector::StorageStrategy::Dense, cbv2->GetStorageStrategy(), ());

  auto cbv3 = coding::CompressedBitVector::Subtract(*cbv1, *cbv2);
  TEST(cbv3.get(), ());
  TEST_EQUAL(coding::CompressedBitVector::StorageStrategy::Sparse, cbv3->GetStorageStrategy(), ());
  CheckSubtraction(setBits1, setBits2, *cbv3);
}

UNIT_TEST(CompressedBitVector_Subtract4)
{
  vector<uint64_t> setBits1 = {0, 5, 15};
  vector<uint64_t> setBits2 = {0, 10};
  auto cbv1 = coding::CompressedBitVectorBuilder::FromBitPositions(setBits1);
  auto cbv2 = coding::CompressedBitVectorBuilder::FromBitPositions(setBits2);
  TEST(cbv1.get(), ());
  TEST(cbv2.get(), ());
  TEST_EQUAL(coding::CompressedBitVector::StorageStrategy::Sparse, cbv1->GetStorageStrategy(), ());
  TEST_EQUAL(coding::CompressedBitVector::StorageStrategy::Sparse, cbv2->GetStorageStrategy(), ());

  auto cbv3 = coding::CompressedBitVector::Subtract(*cbv1, *cbv2);
  TEST(cbv3.get(), ());
  TEST_EQUAL(coding::CompressedBitVector::StorageStrategy::Sparse, cbv3->GetStorageStrategy(), ());
  CheckSubtraction(setBits1, setBits2, *cbv3);
}

UNIT_TEST(CompressedBitVector_SerializationDense)
{
  int const kNumBits = 100;
  vector<uint64_t> setBits;
  for (size_t i = 0; i < kNumBits; ++i)
    setBits.push_back(i);
  vector<uint8_t> buf;
  {
    MemWriter<vector<uint8_t>> writer(buf);
    auto cbv = coding::CompressedBitVectorBuilder::FromBitPositions(setBits);
    TEST_EQUAL(setBits.size(), cbv->PopCount(), ());
    TEST_EQUAL(coding::CompressedBitVector::StorageStrategy::Dense, cbv->GetStorageStrategy(), ());
    cbv->Serialize(writer);
  }
  MemReader reader(buf.data(), buf.size());
  auto cbv = coding::CompressedBitVectorBuilder::Deserialize(reader);
  TEST(cbv.get(), ());
  TEST_EQUAL(coding::CompressedBitVector::StorageStrategy::Dense, cbv->GetStorageStrategy(), ());
  TEST_EQUAL(setBits.size(), cbv->PopCount(), ());
  for (size_t i = 0; i < setBits.size(); ++i)
    TEST(cbv->GetBit(setBits[i]), ());
}

UNIT_TEST(CompressedBitVector_SerializationSparse)
{
  int const kNumBits = 100;
  vector<uint64_t> setBits;
  for (size_t i = 0; i < kNumBits; ++i)
  {
    if (i % 10 == 0)
      setBits.push_back(i);
  }
  vector<uint8_t> buf;
  {
    MemWriter<vector<uint8_t>> writer(buf);
    auto cbv = coding::CompressedBitVectorBuilder::FromBitPositions(setBits);
    TEST_EQUAL(coding::CompressedBitVector::StorageStrategy::Sparse, cbv->GetStorageStrategy(), ());
    cbv->Serialize(writer);
  }
  MemReader reader(buf.data(), buf.size());
  auto cbv = coding::CompressedBitVectorBuilder::Deserialize(reader);
  TEST(cbv.get(), ());
  TEST_EQUAL(coding::CompressedBitVector::StorageStrategy::Sparse, cbv->GetStorageStrategy(), ());
  TEST_EQUAL(setBits.size(), cbv->PopCount(), ());
  for (size_t i = 0; i < setBits.size(); ++i)
    TEST(cbv->GetBit(setBits[i]), ());
}

UNIT_TEST(CompressedBitVector_ForEach)
{
  int const kNumBits = 150;
  vector<uint64_t> denseBits;
  vector<uint64_t> sparseBits;
  for (size_t i = 0; i < kNumBits; ++i)
  {
    denseBits.push_back(i);
    if (i % 15 == 0)
      sparseBits.push_back(i);
  }
  auto denseCBV = coding::CompressedBitVectorBuilder::FromBitPositions(denseBits);
  auto sparseCBV = coding::CompressedBitVectorBuilder::FromBitPositions(sparseBits);
  TEST_EQUAL(coding::CompressedBitVector::StorageStrategy::Dense, denseCBV->GetStorageStrategy(),
             ());
  TEST_EQUAL(coding::CompressedBitVector::StorageStrategy::Sparse, sparseCBV->GetStorageStrategy(),
             ());

  set<uint64_t> denseSet;
  uint64_t maxPos = 0;
  coding::CompressedBitVectorEnumerator::ForEach(*denseCBV, [&](uint64_t pos)
                                                 {
                                                   denseSet.insert(pos);
                                                   maxPos = max(maxPos, pos);
                                                 });
  TEST_EQUAL(denseSet.size(), kNumBits, ());
  TEST_EQUAL(maxPos, kNumBits - 1, ());

  coding::CompressedBitVectorEnumerator::ForEach(*sparseCBV, [](uint64_t pos)
                                                 {
                                                   TEST_EQUAL(pos % 15, 0, ());
                                                 });
}

UNIT_TEST(CompressedBitVector_DenseOneBit)
{
  vector<uint64_t> setBits = {0};
  unique_ptr<coding::DenseCBV> cbv(new coding::DenseCBV(setBits));
  TEST_EQUAL(cbv->PopCount(), 1, ());
  coding::CompressedBitVectorEnumerator::ForEach(*cbv, [&](uint64_t pos)
                                                 {
                                                   TEST_EQUAL(pos, 0, ());
                                                 });
}
