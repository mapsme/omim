#include "storage/index.hpp"

#include "std/sstream.hpp"

namespace storage
{
storage::TIndex const kInvalidIndex;

bool IsIndexValid(TIndex const & index)
{
  return index != kInvalidIndex;
}
} //  namespace storage
