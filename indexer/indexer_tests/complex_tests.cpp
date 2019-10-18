#include "testing/testing.hpp"

#include "generator/generator_tests_support/test_with_classificator.hpp"

#include "indexer/classificator.hpp"
#include "indexer/classificator_loader.hpp"
#include "indexer/complex/complex.hpp"
#include "indexer/complex/hierarchy_entry.hpp"
#include "indexer/complex/serdes.hpp"
#include "indexer/complex/tree_node.hpp"
#include "indexer/composite_id.hpp"

#include "coding/csv_reader.hpp"

#include "base/geo_object_id.hpp"

#include "platform/platform_tests_support/scoped_file.hpp"

using generator::tests_support::TestWithClassificator;
using platform::tests_support::ScopedFile;

namespace
{
std::string const kCsv1 =
    "13835058055284963881 9223372037111861697;"
    ";"
    "1;"
    "37.5303271;"
    "67.3684086;"
    "amenity-university;"
    "Lomonosov Moscow State Univesity;"
    "Russia_Moscow\n"

    "9223372036879747192 9223372036879747192;"
    "13835058055284963881 9223372037111861697;"
    "2;"
    "37.5272372;"
    "67.3775872;"
    "leisure-garden;"
    "Ботанический сад МГУ;"
    "Russia_Moscow\n"

    "9223372036938640141 9223372036938640141;"
    "9223372036879747192 9223372036879747192;"
    "3;"
    "37.5274156;"
    "67.3758813;"
    "amenity-university;"
    "Отдел флоры;"
    "Russia_Moscow\n"

    "9223372036964008573 9223372036964008573;"
    "9223372036879747192 9223372036879747192;"
    "3;"
    "37.5279467;"
    "67.3756452;"
    "amenity-university;"
    "Дендрарий Ботанического сада МГУ;"
    "Russia_Moscow\n"

    "4611686019330739245 4611686019330739245;"
    "13835058055284963881 9223372037111861697;"
    "2;"
    "37.5357492;"
    "67.3735142;"
    "historic-memorial;"
    "Александр Иванович Герцен;"
    "Russia_Moscow\n"

    "4611686019330739269 4611686019330739269;"
    "13835058055284963881 9223372037111861697;"
    "2;"
    "37.5351269;"
    "67.3741606;"
    "historic-memorial;"
    "Николай Гаврилович Чернышевский;"
    "Russia_Moscow\n"

    "4611686019330739276 4611686019330739276;"
    "13835058055284963881 9223372037111861697;"
    "2;"
    "37.5345234;"
    "67.3723206;"
    "historic-memorial;"
    "Николай Егорович Жуковский;"
    "Russia_Moscow";

indexer::CompositeId MakeId(unsigned long long f, unsigned long long s)
{
  return indexer::CompositeId(base::GeoObjectId(static_cast<uint64_t>(f)),
                              base::GeoObjectId(static_cast<uint64_t>(s)));
}

UNIT_CLASS_TEST(TestWithClassificator, Complex_HierarchyEntryCsv)
{
  indexer::HierarchyEntry e;
  e.m_id = MakeId(4611686018725364866ull, 4611686018725364866ull);
  e.m_parentId = MakeId(13835058055283414237ull, 9223372037374119493ull);
  e.m_depth = 2;
  e.m_center.x = 37.6262604;
  e.m_center.y = 67.6098812;
  e.m_type = classif().GetTypeByPath({"amenity", "restaurant"});
  e.m_name = "Rybatskoye podvor'ye";
  e.m_countryName = "Russia_Moscow";

  auto const row = indexer::hierarchy::HierarchyEntryToCsvRow(e);
  TEST_EQUAL(row.size(), 8, ());
  TEST_EQUAL(row[0], "4611686018725364866 4611686018725364866", ());
  TEST_EQUAL(row[1], "13835058055283414237 9223372037374119493", ());
  TEST_EQUAL(row[2], "2", ());
  TEST_EQUAL(row[3], "37.6262604", ());
  TEST_EQUAL(row[4], "67.6098812", ());
  TEST_EQUAL(row[5], "amenity-restaurant", ());
  TEST_EQUAL(row[6], "Rybatskoye podvor'ye", ());
  TEST_EQUAL(row[7], "Russia_Moscow", ());

  auto const res = indexer::hierarchy::HierarchyEntryFromCsvRow(row);
  TEST_EQUAL(e, res, ());
}

UNIT_CLASS_TEST(TestWithClassificator, Complex_LoadHierachy)
{
  auto const filename = "test.csv";
  ScopedFile sf(filename, kCsv1);
  auto const forest = indexer::hierarchy::LoadHierachy(sf.GetFullPath());
  TEST_EQUAL(forest.size(), 1, ());
  auto const & tree = *forest.begin();
  LOG(LINFO, (tree));

  TEST_EQUAL(tree_node::Size(tree), 7, ());
  auto node = tree_node::FindIf(tree, [](auto const & e) {
    return e.m_id == MakeId(13835058055284963881ull, 9223372037111861697ull);
  });
  TEST(node, ());
  TEST(!node->HasParent(), ());
  TEST_EQUAL(node->GetChildren().size(), 4, ());

  node = tree_node::FindIf(tree, [](auto const & e) {
    return e.m_id == MakeId(9223372036879747192ull, 9223372036879747192ull);
  });
  TEST(node, ());
  TEST(node->HasParent(), ());
  TEST_EQUAL(node->GetParent()->GetData().m_id,
             MakeId(13835058055284963881ull, 9223372037111861697ull), ());
  TEST_EQUAL(node->GetChildren().size(), 2, ());

  node = tree_node::FindIf(tree, [](auto const & e) {
    return e.m_id == MakeId(9223372036938640141ull, 9223372036938640141ull);
  });
  TEST(node, ());
  TEST_EQUAL(node->GetData().m_depth, tree_node::GetDepth(node), ());
  TEST_EQUAL(node->GetParent()->GetData().m_id,
             MakeId(9223372036879747192ull, 9223372036879747192ull), ());
  TEST_EQUAL(node->GetChildren().size(), 0, ());
}

UNIT_CLASS_TEST(TestWithClassificator, Complex_IsComplex)
{
  auto const filename = "test.csv";
  ScopedFile sf(filename, kCsv1);
  auto const forest = indexer::hierarchy::LoadHierachy(sf.GetFullPath());
  TEST_EQUAL(forest.size(), 1, ());
  auto const & tree = *forest.begin();
  TEST(indexer::IsComplex(tree), ());
}

UNIT_CLASS_TEST(TestWithClassificator, Complex_GetCountry)
{
  auto const filename = "test.csv";
  ScopedFile sf(filename, kCsv1);
  auto const forest = indexer::hierarchy::LoadHierachy(sf.GetFullPath());
  TEST_EQUAL(forest.size(), 1, ());
  auto const & tree = *forest.begin();
  TEST_EQUAL(indexer::GetCountry(tree), "Russia_Moscow", ());
}

UNIT_CLASS_TEST(TestWithClassificator, Complex_ComplexLoader)
{
  auto const filename = "test.csv";
  ScopedFile sf(filename, kCsv1);
  indexer::SourceComplexesLoader const loader(sf.GetFullPath());
  auto const forest = loader.GetForest("Russia_Moscow");
  TEST_EQUAL(forest.Size(), 1, ());
  forest.ForEachTree([](auto const & tree) { TEST_EQUAL(tree_node::Size(tree), 7, ()); });
}

UNIT_CLASS_TEST(TestWithClassificator, Complex_Serdes)
{
  auto tree1 = tree_node::MakeTreeNode<indexer::ComplexSerdes::Id>(1);
  auto node11 = tree_node::MakeTreeNode<indexer::ComplexSerdes::Id>(11);
  tree_node::Link(tree_node::MakeTreeNode<indexer::ComplexSerdes::Id>(111), node11);
  tree_node::Link(tree_node::MakeTreeNode<indexer::ComplexSerdes::Id>(112), node11);
  tree_node::Link(tree_node::MakeTreeNode<indexer::ComplexSerdes::Id>(113), node11);
  tree_node::Link(node11, tree1);
  tree_node::Link(tree_node::MakeTreeNode<indexer::ComplexSerdes::Id>(12), tree1);

  auto tree2 = tree_node::MakeTreeNode<indexer::ComplexSerdes::Id>(2);
  tree_node::Link(tree_node::MakeTreeNode<indexer::ComplexSerdes::Id>(21), tree2);
  tree_node::Link(tree_node::MakeTreeNode<indexer::ComplexSerdes::Id>(22), tree2);


  tree_node::Forest<indexer::ComplexSerdes::Id> expectedForest;
  expectedForest.Append(tree1);
  expectedForest.Append(tree2);

  using ByteVector = std::vector<uint8_t>;

  ByteVector buffer;
  MemWriter<decltype(buffer)> writer(buffer);
  WriterSink<decltype(writer)> sink(writer);
  indexer::ComplexSerdes::Serialize(sink, indexer::ComplexSerdes::Version::V0, expectedForest);

  MemReader reader(buffer.data(), buffer.size());
  ReaderSource<decltype(reader)> src(reader);
  tree_node::Forest<indexer::ComplexSerdes::Id> forest;
  TEST(indexer::ComplexSerdes::Deserialize(src, indexer::ComplexSerdes::Version::V0, forest), ());
  LOG(LINFO, (forest));
  LOG(LINFO, (expectedForest));
  TEST_EQUAL(forest, expectedForest, ());
}

UNIT_CLASS_TEST(TestWithClassificator, Complex_TransformToTree)
{
  auto const filename = "test.csv";
  ScopedFile sf(filename, kCsv1);
  auto const forest = indexer::hierarchy::LoadHierachy(sf.GetFullPath());
  TEST(!forest.empty(), ());
  auto const & tree = forest.front();
  auto transformedTree = tree_node::TransformToTree(tree, [](auto const & entry) {
    return entry.m_name;
  });

  auto expectedTree = tree_node::MakeTreeNode<std::string>("Lomonosov Moscow State Univesity");
  tree_node::Link(tree_node::MakeTreeNode<std::string>("Николай Егорович Жуковский"), expectedTree);
  auto const node1 = tree_node::MakeTreeNode<std::string>("Ботанический сад МГУ");
  tree_node::Link(tree_node::MakeTreeNode<std::string>("Отдел флоры"), node1);
  tree_node::Link(tree_node::MakeTreeNode<std::string>("Дендрарий Ботанического сада МГУ"), node1);
  tree_node::Link(node1, expectedTree);
  tree_node::Link(tree_node::MakeTreeNode<std::string>("Александр Иванович Герцен"), expectedTree);
  tree_node::Link(tree_node::MakeTreeNode<std::string>("Николай Гаврилович Чернышевский"), expectedTree);
  TEST(tree_node::IsEqual(transformedTree, expectedTree), ());
}
}  // namespace
