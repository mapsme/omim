#include "indexer/complex/serdes.hpp"

namespace indexer
{
// static
uint32_t const ComplexSerdes::kHeaderMagic = 0xDACEFADE;

// static
ComplexSerdes::Version const ComplexSerdes::kLatestVersion = Version::V0;
}  // namespace indexer
