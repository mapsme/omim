#pragma once

#include "geocoder/hierarchy.hpp"

#include "base/geo_object_id.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace geocoder
{
class Index
{
public:
  // Number of the entry in the list of all hierarchy entries
  // that the index was constructed from.
  using DocId = uint32_t;

  using Doc = Hierarchy::Entry;

  explicit Index(Hierarchy const & hierarchy);

  Doc const & GetDoc(DocId const id) const;

  // Calls |fn| for DocIds of Docs whose names exactly match |tokens| (the order matters).
  //
  // todo This method (and the whole class, in fact) is in the
  //      prototype stage and may be too slow. Proper indexing should
  //      be implemented to perform this type of queries.
  template <typename Fn>
  void ForEachDocId(Tokens const & tokens, Fn && fn) const
  {
    auto const it = m_docIdsByTokens.find(MakeIndexKey(tokens));
    if (it == m_docIdsByTokens.end())
      return;

    for (DocId const & docId : it->second)
      fn(docId);
  }

  // Calls |fn| for DocIds of buildings that are located on the
  // street whose DocId is |streetDocId|.
  template <typename Fn>
  void ForEachBuildingOnStreet(DocId const & streetDocId, Fn && fn) const
  {
    auto const it = m_buildingsOnStreet.find(streetDocId);
    if (it == m_buildingsOnStreet.end())
      return;

    for (DocId const & docId : it->second)
      fn(docId);
  }

private:
  // Converts |tokens| to a single UTF-8 string that can be used
  // as a key in the |m_docIdsByTokens| map.
  std::string MakeIndexKey(Tokens const & tokens) const;

  // Adds address information of |m_docs| to the index.
  void AddEntries();

  // Adds the street |e| (which has the id of |docId|) to the index,
  // with and without synonyms of the word "street".
  void AddStreet(DocId const & docId, Doc const & e);

  // Fills the |m_buildingsOnStreet| field.
  void AddHouses(Hierarchy const & hierarchy);

  std::vector<Doc> const & m_docs;

  std::unordered_map<std::string, std::vector<DocId>> m_docIdsByTokens;

  // Lists of houses grouped by the streets they belong to.
  std::unordered_map<DocId, std::vector<DocId>> m_buildingsOnStreet;
};
}  // namespace geocoder
