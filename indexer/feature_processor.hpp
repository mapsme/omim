#pragma once

#include "indexer/features_tag.hpp"
#include "indexer/features_vector.hpp"

#include "coding/file_reader.hpp"
#include "coding/files_container.hpp"

#include <memory>
#include <string>
#include <utility>

namespace feature
{
template <class ToDo>
void ForEachFeature(ModelReaderPtr reader, ToDo && toDo, bool includeIsolines = true)
{
  FeaturesVectorTest features((FilesContainerR(reader)), FeaturesTag::Common);
  features.GetVector().ForEach(std::forward<ToDo>(toDo));
  if (includeIsolines)
  {
    FeaturesVectorTest isolineFeatures((FilesContainerR(reader)), FeaturesTag::Isolines);
    isolineFeatures.GetVector().ForEach(std::forward<ToDo>(toDo));
  }
}

template <class ToDo>
void ForEachFeature(std::string const & fPath, ToDo && toDo, bool includeIsolines = true)
{
  ForEachFeature(std::make_unique<FileReader>(fPath), std::forward<ToDo>(toDo), includeIsolines);
}
}  // namespace feature
