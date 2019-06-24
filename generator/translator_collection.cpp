#include "generator/translator_collection.hpp"

#include "generator/osm_element.hpp"

#include <algorithm>
#include <iterator>

namespace generator
{
std::shared_ptr<TranslatorInterface>
TranslatorCollection::Clone(std::shared_ptr<cache::IntermediateData> const & cache) const
{
  auto p = std::make_shared<TranslatorCollection>();
  for (auto const & c : m_collection)
   p->Append(c->Clone(cache));
  return p;
}

void TranslatorCollection::Emit(OsmElement /* const */ & element)
{
  for (auto & t : m_collection)
  {
    OsmElement copy = element;
    t->Emit(copy);
  }
}

void TranslatorCollection::Flush()
{
  for (auto & t : m_collection)
    t->Flush();
}

bool TranslatorCollection::Finish()
{
  return std::all_of(std::begin(m_collection), std::end(m_collection), [](auto & t) {
    return t->Finish();
  });
}

void TranslatorCollection::Merge(TranslatorInterface const * collector)
{
  CHECK(collector, ());

  collector->MergeInto(const_cast<TranslatorCollection *>(this));
}

void TranslatorCollection::MergeInto(TranslatorCollection * collector) const
{
  CHECK(collector, ());

  auto & otherCollection = collector->m_collection;
  CHECK_EQUAL(m_collection.size(), otherCollection.size(), ());
  for (size_t i = 0; i < m_collection.size(); ++i)
    otherCollection[i]->Merge(m_collection[i].get());
}
}  // namespace generator
