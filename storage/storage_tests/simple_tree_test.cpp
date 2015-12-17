#include "testing/testing.hpp"

#include "storage/country.hpp"
#include "storage/simple_tree.hpp"

#include "std/string.hpp"

using namespace storage;

namespace
{

template <class TNode>
struct Calculator
{
  size_t count;
  Calculator() : count(0) {}
  void operator()(TNode const &)
  {
    ++count;
  }
};

}

UNIT_TEST(SimpleTree_Smoke)
{
  typedef SimpleTree<int> TreeT;
  TreeT tree;

  tree.Add(4);
  tree.Add(3);
  tree.Add(5);
  tree.Add(2);
  tree.Add(1);
  tree.AddAtDepth(1, 20);  // 1 is parent
  tree.AddAtDepth(1, 10);  // 1 is parent
  tree.AddAtDepth(1, 30);  // 1 is parent

  TEST(!tree.Find(100), ());

  TreeT const * node = tree.Find(1);
  TEST(node, ());
  TEST_EQUAL(node->Value(), 1, ());
  TEST(node->Find(10), ());
  TEST_EQUAL((*node)[0].Value(), 20, ());

  node = tree.Find(10);
  TEST(node, ());
  TEST_EQUAL(node->Value(), 10, ());

  tree.Sort();
  // test sorting
  TEST_EQUAL(tree[0].Value(), 1, ());
  TEST_EQUAL(tree[1].Value(), 2, ());
  TEST_EQUAL(tree[2].Value(), 3, ());
  TEST_EQUAL(tree[3].Value(), 4, ());
  TEST_EQUAL(tree[4].Value(), 5, ());
  TEST_EQUAL(tree[0][0].Value(), 10, ());
  TEST_EQUAL(tree[0][1].Value(), 20, ());
  TEST_EQUAL(tree[0][2].Value(), 30, ());

  Calculator<TreeT> c1;
  tree.ForEachSibling(c1);
  TEST_EQUAL(c1.count, 5, ());

  Calculator<TreeT> c2;
  tree.ForEachChildren(c2);
  TEST_EQUAL(c2.count, 8, ());

  tree.Clear();
  Calculator<TreeT> c3;
  tree.ForEachChildren(c3);
  TEST_EQUAL(c3.count, 0, ("Tree should be empty"));
}

UNIT_TEST(SimpleTree_Country)
{
  typedef SimpleTree<Country> TSimpleTreeCountry;
  TSimpleTreeCountry tree;

  tree.Add(Country("belarus"));
  tree.Add(Country("russia"));
  tree.AddAtDepth(1, Country("moscow"));
  tree.AddAtDepth(1, Country("saint_petersburg"));
  tree.AddAtDepth(1, Country("yekaterinburg"));

  TSimpleTreeCountry const * node = tree.Find(Country("moscow"));
  TEST(node, ());
  TEST_EQUAL(node->Value().Name(), "moscow", ());
  TEST_EQUAL(node->SiblingsCount(), 0, ());

  node = tree.Find(Country("belarus"));
  TEST(node, ());
  TEST_EQUAL(node->Value().Name(), "belarus", ());
  TEST_EQUAL(node->SiblingsCount(), 0, ());

  node = tree.Find(Country("russia"));
  TEST(node, ());
  TEST_EQUAL(node->Value().Name(), "russia", ());
  TEST_EQUAL(node->SiblingsCount(), 3, ());
  TEST(node->Find(Country("yekaterinburg")), ());

  node = tree.Find(Country("saint_petersburg"));
  TEST(node, ());
  TEST_EQUAL(node->Value().Name(), "saint_petersburg", ());
  TEST_EQUAL(node->SiblingsCount(), 0, ());

  node = tree.FindLeaf(Country("saint_petersburg"));
  TEST(node, ());
  TEST_EQUAL(node->Value().Name(), "saint_petersburg", ());
  TEST_EQUAL(node->SiblingsCount(), 0, ());
}
