#include "storage/index.hpp"

#include "std/sstream.hpp"


namespace storage
{
bool IsIndexValid(TIndex const & index)
{
  return index != kInvalidIndex;
}
} //  namespace storage
