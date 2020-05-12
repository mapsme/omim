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
void ForEachCommonFeature(ModelReaderPtr reader, ToDo && toDo)
{
  FeaturesVectorTest features((FilesContainerR(reader)), FeaturesTag::Common);
  features.GetVector().ForEach(std::forward<ToDo>(toDo));
}

template <class ToDo>
void ForEachCommonFeature(std::string const & fPath, ToDo && toDo)
{
  ForEachCommonFeature(std::make_unique<FileReader>(fPath), std::forward<ToDo>(toDo));
}
}  // namespace feature
