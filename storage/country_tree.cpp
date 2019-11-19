#include "storage/country_tree.hpp"

#include "platform/mwm_version.hpp"
#include "platform/platform.hpp"

#include "coding/reader.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"
#include "base/stl_helpers.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>

#include "3party/jansson/myjansson.hpp"

using namespace std;
using platform::CountryFile;

namespace storage
{
// Mwm subtree attributes. They can be calculated based on information contained in countries.txt.
// The first in the pair is number of mwms in a subtree. The second is sum of sizes of
// all mwms in a subtree.
using MwmSubtreeAttrs = pair<MwmCounter, MwmSize>;

namespace
{
class StoreInterface
{
public:
  virtual ~StoreInterface() = default;
  virtual Country * InsertToCountryTree(CountryId const & id, MwmSize mapSize,
                                        string const & mapSha1, size_t depth,
                                        CountryId const & parent) = 0;
  virtual void InsertOldMwmMapping(CountryId const & newId, CountryId const & oldId) = 0;
  virtual void InsertAffiliation(CountryId const & countryId, string const & affilation) = 0;
  virtual void InsertCountryNameSynonym(CountryId const & countryId, string const & synonym) = 0;
  virtual void InsertMwmTopCityGeoId(CountryId const & countryId, uint64_t const & geoObjectId) {}
  virtual void InsertTopCountryGeoIds(CountryId const & countryId,
                                      vector<uint64_t> const & geoObjectIds)
  {
  }
  virtual OldMwmMapping GetMapping() const = 0;
};

class StoreCountries : public StoreInterface
{
  CountryTree & m_countries;
  Affiliations & m_affiliations;
  CountryNameSynonyms & m_countryNameSynonyms;
  MwmTopCityGeoIds & m_mwmTopCityGeoIds;
  MwmTopCountryGeoIds & m_mwmTopCountryGeoIds;
  OldMwmMapping m_idsMapping;

public:
  StoreCountries(CountryTree & countries, Affiliations & affiliations,
                 CountryNameSynonyms & countryNameSynonyms, MwmTopCityGeoIds & mwmTopCityGeoIds,
                 MwmTopCountryGeoIds & mwmTopCountryGeoIds)
    : m_countries(countries)
    , m_affiliations(affiliations)
    , m_countryNameSynonyms(countryNameSynonyms)
    , m_mwmTopCityGeoIds(mwmTopCityGeoIds)
    , m_mwmTopCountryGeoIds(mwmTopCountryGeoIds)
  {
  }
  ~StoreCountries()
  {
    for (auto & entry : m_affiliations)
      base::SortUnique(entry.second);
  }

  // StoreInterface overrides:
  Country * InsertToCountryTree(CountryId const & id, MwmSize mapSize, string const & mapSha1,
                                size_t depth, CountryId const & parent) override
  {
    Country country(id, parent);
    if (mapSize)
    {
      CountryFile countryFile(id);
      countryFile.SetRemoteSize(mapSize);
      countryFile.SetSha1(mapSha1);
      country.SetFile(countryFile);
    }
    return &m_countries.AddAtDepth(depth, country);
  }

  void InsertOldMwmMapping(CountryId const & newId, CountryId const & oldId) override
  {
    m_idsMapping[oldId].insert(newId);
  }

  void InsertAffiliation(CountryId const & countryId, string const & affilation) override
  {
    ASSERT(!affilation.empty(), ());
    ASSERT(!countryId.empty(), ());

    m_affiliations[affilation].push_back(countryId);
  }

  void InsertCountryNameSynonym(CountryId const & countryId, string const & synonym) override
  {
    ASSERT(!synonym.empty(), ());
    ASSERT(!countryId.empty(), ());
    ASSERT(m_countryNameSynonyms.find(synonym) == m_countryNameSynonyms.end(),
           ("Synonym must identify CountryTree node where the country is located. Country cannot be "
            "located at multiple nodes."));

    m_countryNameSynonyms[synonym] = countryId;
  }

  void InsertMwmTopCityGeoId(CountryId const & countryId, uint64_t const & geoObjectId) override
  {
    ASSERT(!countryId.empty(), ());
    ASSERT_NOT_EQUAL(geoObjectId, 0, ());
    base::GeoObjectId id(geoObjectId);
    m_mwmTopCityGeoIds.emplace(countryId, move(id));
  }

  void InsertTopCountryGeoIds(CountryId const & countryId,
                              vector<uint64_t> const & geoObjectIds) override
  {
    ASSERT(!countryId.empty(), ());
    ASSERT(!geoObjectIds.empty(), ());
    vector<base::GeoObjectId> ids(geoObjectIds.cbegin(), geoObjectIds.cend());
    m_mwmTopCountryGeoIds.emplace(countryId, move(ids));
  }

  OldMwmMapping GetMapping() const override { return m_idsMapping; }
};

class StoreFile2Info : public StoreInterface
{
  OldMwmMapping m_idsMapping;
  map<string, CountryInfo> & m_file2info;

public:
  explicit StoreFile2Info(map<string, CountryInfo> & file2info) : m_file2info(file2info)
  {
  }
  // StoreInterface overrides:
  Country * InsertToCountryTree(CountryId const & id, MwmSize /* mapSize */,
                                string const & /* mapSha1 */, size_t /* depth */,
                                CountryId const & /* parent */) override
  {
    CountryInfo info(id);
    m_file2info[id] = move(info);
    return nullptr;
  }

  void InsertOldMwmMapping(CountryId const & /* newId */, CountryId const & /* oldId */) override {}

  void InsertAffiliation(CountryId const & /* countryId */,
                         string const & /* affilation */) override
  {
  }

  void InsertCountryNameSynonym(CountryId const & /* countryId */,
                                string const & /* synonym */) override
  {
  }

  OldMwmMapping GetMapping() const override
  {
    ASSERT(false, ());
    return map<CountryId, CountriesSet>();
  }
};
}  // namespace

// CountryTree::Node -------------------------------------------------------------------------------

CountryTree::Node * CountryTree::Node::AddAtDepth(size_t level, Country const & value)
{
  Node * node = this;
  while (--level > 0 && !node->m_children.empty())
    node = node->m_children.back().get();
  ASSERT_EQUAL(level, 0, ());
  return node->Add(value);
}

CountryTree::Node const & CountryTree::Node::Parent() const
{
  CHECK(HasParent(), ());
  return *m_parent;
}

CountryTree::Node const & CountryTree::Node::Child(size_t index) const
{
  ASSERT_LESS(index, m_children.size(), ());
  return *m_children[index];
}

void CountryTree::Node::ForEachChild(CountryTree::Node::NodeCallback const & f)
{
  for (auto & child : m_children)
    f(*child);
}

void CountryTree::Node::ForEachChild(CountryTree::Node::NodeCallback const & f) const
{
  for (auto const & child : m_children)
    f(*child);
}

void CountryTree::Node::ForEachDescendant(CountryTree::Node::NodeCallback const & f)
{
  for (auto & child : m_children)
  {
    f(*child);
    child->ForEachDescendant(f);
  }
}

void CountryTree::Node::ForEachDescendant(CountryTree::Node::NodeCallback const & f) const
{
  for (auto const & child : m_children)
  {
    f(*child);
    child->ForEachDescendant(f);
  }
}

void CountryTree::Node::ForEachInSubtree(CountryTree::Node::NodeCallback const & f)
{
  f(*this);
  for (auto & child : m_children)
    child->ForEachInSubtree(f);
}

void CountryTree::Node::ForEachInSubtree(CountryTree::Node::NodeCallback const & f) const
{
  f(*this);
  for (auto const & child : m_children)
    child->ForEachInSubtree(f);
}

void CountryTree::Node::ForEachAncestorExceptForTheRoot(CountryTree::Node::NodeCallback const & f)
{
  if (m_parent == nullptr || m_parent->m_parent == nullptr)
    return;
  f(*m_parent);
  m_parent->ForEachAncestorExceptForTheRoot(f);
}

void CountryTree::Node::ForEachAncestorExceptForTheRoot(
    CountryTree::Node::NodeCallback const & f) const
{
  if (m_parent == nullptr || m_parent->m_parent == nullptr)
    return;
  f(*m_parent);
  m_parent->ForEachAncestorExceptForTheRoot(f);
}

CountryTree::Node * CountryTree::Node::Add(Country const & value)
{
  m_children.emplace_back(std::make_unique<Node>(value, this));
  return m_children.back().get();
}

// CountryTree -------------------------------------------------------------------------------------

CountryTree::Node const & CountryTree::GetRoot() const
{
  CHECK(m_countryTree, ());
  return *m_countryTree;
}

CountryTree::Node & CountryTree::GetRoot()
{
  CHECK(m_countryTree, ());
  return *m_countryTree;
}

Country & CountryTree::AddAtDepth(size_t level, Country const & value)
{
  Node * added = nullptr;
  if (level == 0)
  {
    ASSERT(IsEmpty(), ());
    m_countryTree = std::make_unique<Node>(value, nullptr);  // Creating the root node.
    added = m_countryTree.get();
  }
  else
  {
    added = m_countryTree->AddAtDepth(level, value);
  }

  ASSERT(added, ());
  m_countryTreeMap.insert(make_pair(value.Name(), added));
  return added->Value();
}

void CountryTree::Clear()
{
  m_countryTree.reset();
  m_countryTreeMap.clear();
}

void CountryTree::Find(CountryId const & key, vector<Node const *> & found) const
{
  found.clear();
  if (IsEmpty())
    return;

  if (key == m_countryTree->Value().Name())
    found.push_back(m_countryTree.get());

  auto const range = m_countryTreeMap.equal_range(key);
  for (auto it = range.first; it != range.second; ++it)
    found.push_back(it->second);
}

CountryTree::Node const * const CountryTree::FindFirst(CountryId const & key) const
{
  if (IsEmpty())
    return nullptr;

  vector<Node const *> found;
  Find(key, found);
  if (found.empty())
    return nullptr;
  return found[0];
}

CountryTree::Node const * const CountryTree::FindFirstLeaf(CountryId const & key) const
{
  if (IsEmpty())
    return nullptr;

  vector<Node const *> found;
  Find(key, found);

  for (auto node : found)
  {
    if (node->ChildrenCount() == 0)
      return node;
  }
  return nullptr;
}

MwmSubtreeAttrs LoadGroupImpl(size_t depth, json_t * node, CountryId const & parent,
                              StoreInterface & store)
{
  CountryId id;
  FromJSONObject(node, "id", id);

  vector<string> countryNameSynonyms;
  FromJSONObjectOptionalField(node, "country_name_synonyms", countryNameSynonyms);
  for (auto const & synonym : countryNameSynonyms)
    store.InsertCountryNameSynonym(id, synonym);

  vector<string> affiliations;
  FromJSONObjectOptionalField(node, "affiliations", affiliations);
  for (auto const & affilationValue : affiliations)
    store.InsertAffiliation(id, affilationValue);

  uint64_t geoObjectId = 0;
  FromJSONObjectOptionalField(node, "top_city_geo_id", geoObjectId);
  if (geoObjectId != 0)
    store.InsertMwmTopCityGeoId(id, geoObjectId);

  vector<uint64_t> topCountryIds;
  FromJSONObjectOptionalField(node, "top_countries_geo_ids", topCountryIds);
  if (!topCountryIds.empty())
    store.InsertTopCountryGeoIds(id, topCountryIds);

  int nodeSize;
  FromJSONObjectOptionalField(node, "s", nodeSize);
  ASSERT_LESS_OR_EQUAL(0, nodeSize, ());

  string nodeHash;
  FromJSONObjectOptionalField(node, "sha1_base64", nodeHash);

  // We expect that mwm and routing files should be less than 2GB.
  Country * addedNode = store.InsertToCountryTree(id, nodeSize, nodeHash, depth, parent);

  MwmCounter mwmCounter = 0;
  MwmSize mwmSize = 0;
  vector<json_t *> children;
  FromJSONObjectOptionalField(node, "g", children);
  if (children.empty())
  {
    mwmCounter = 1;  // It's a leaf. Any leaf contains one mwm.
    mwmSize = nodeSize;
  }
  else
  {
    for (json_t * child : children)
    {
      MwmSubtreeAttrs const childAttr = LoadGroupImpl(depth + 1, child, id, store);
      mwmCounter += childAttr.first;
      mwmSize += childAttr.second;
    }
  }

  if (addedNode != nullptr)
    addedNode->SetSubtreeAttrs(mwmCounter, mwmSize);

  return make_pair(mwmCounter, mwmSize);
}

bool LoadCountriesImpl(string const & jsonBuffer, StoreInterface & store)
{
  try
  {
    base::Json root(jsonBuffer.c_str());
    LoadGroupImpl(0 /* depth */, root.get(), kInvalidCountryId, store);
    return true;
  }
  catch (base::Json::Exception const & e)
  {
    LOG(LERROR, (e.Msg()));
    return false;
  }
}

int64_t LoadCountriesFromBuffer(string const & jsonBuffer, CountryTree & countries,
                                Affiliations & affiliations,
                                CountryNameSynonyms & countryNameSynonyms,
                                MwmTopCityGeoIds & mwmTopCityGeoIds,
                                MwmTopCountryGeoIds & mwmTopCountryGeoIds)
{
  countries.Clear();
  affiliations.clear();

  int64_t version = -1;
  try
  {
    base::Json root(jsonBuffer.c_str());
    FromJSONObject(root.get(), "v", version);

    StoreCountries store(countries, affiliations, countryNameSynonyms, mwmTopCityGeoIds,
                         mwmTopCountryGeoIds);
    if (!LoadCountriesImpl(jsonBuffer, store))
      return -1;
  }
  catch (base::Json::Exception const & e)
  {
    LOG(LERROR, (e.Msg()));
  }
  return version;
}

int64_t LoadCountriesFromFile(string const & path, CountryTree & countries,
                              Affiliations & affiliations,
                              CountryNameSynonyms & countryNameSynonyms,
                              MwmTopCityGeoIds & mwmTopCityGeoIds,
                              MwmTopCountryGeoIds & mwmTopCountryGeoIds)
{
  string json;
  ReaderPtr<Reader>(GetPlatform().GetReader(path)).ReadAsString(json);
  return LoadCountriesFromBuffer(json, countries, affiliations, countryNameSynonyms,
                                 mwmTopCityGeoIds, mwmTopCountryGeoIds);
}

void LoadCountryFile2CountryInfo(string const & jsonBuffer, map<string, CountryInfo> & id2info)
{
  ASSERT(id2info.empty(), ());

  int64_t version = -1;
  try
  {
    base::Json root(jsonBuffer.c_str());
    FromJSONObjectOptionalField(root.get(), "v", version);

    StoreFile2Info store(id2info);
    LoadCountriesImpl(jsonBuffer, store);
  }
  catch (base::Json::Exception const & e)
  {
    LOG(LERROR, (e.Msg()));
  }
}
}  // namespace storage
