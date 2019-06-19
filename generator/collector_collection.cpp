#include "generator/collector_collection.hpp"

#include "generator/feature_builder.hpp"
#include "generator/intermediate_elements.hpp"
#include "generator/osm_element.hpp"

using namespace feature;

namespace generator
{
std::shared_ptr<CollectorInterface>
CollectorCollection::Clone(std::shared_ptr<cache::IntermediateDataReader> const & cache) const
{
  auto p = std::make_shared<CollectorCollection>();
  for (auto const & c : m_collection)
   p->Append(c->Clone(cache));
  return p;
}

void CollectorCollection::Collect(OsmElement const & element)
{
  for (auto & c : m_collection)
    c->Collect(element);
}

void CollectorCollection::CollectRelation(RelationElement const & element)
{
  for (auto & c : m_collection)
    c->CollectRelation(element);
}

void CollectorCollection::CollectFeature(FeatureBuilder const & feature, OsmElement const & element)
{
  for (auto & c : m_collection)
    c->CollectFeature(feature, element);
}

void CollectorCollection::Save()
{
  for (auto & c : m_collection)
    c->Save();
}

void CollectorCollection::Merge(CollectorInterface const * collector)
{
  CHECK(collector, ());

  collector->MergeInto(const_cast<CollectorCollection *>(this));
}

void CollectorCollection::MergeInto(CollectorCollection * collector) const
{
  CHECK(collector, ());

  auto & otherCollection = collector->m_collection;
  CHECK_EQUAL(m_collection.size(), otherCollection.size(), ());
  for (size_t i = 0; i < m_collection.size(); ++i)
    otherCollection[i]->Merge(m_collection[i].get());
}
}  // namespace generator
