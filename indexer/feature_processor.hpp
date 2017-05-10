#pragma once

#include "indexer/data_header.hpp"
#include "indexer/features_vector.hpp"

#include "defines.hpp"

#include "coding/file_reader.hpp"
#include "coding/file_container.hpp"

#include <functional>


namespace feature
{
template <class ToDo>
void ForEachFromDat(ModelReaderPtr reader, ToDo && toDo)
  {
    FeaturesVectorTest features((FilesContainerR(reader)));
    features.GetVector().ForEach(std::ref(toDo));
  }

  template <class ToDo>
  void ForEachFromDat(std::string const & fPath, ToDo && toDo)
  {
    ForEachFromDat(make_unique<FileReader>(fPath), toDo);
  }
}
