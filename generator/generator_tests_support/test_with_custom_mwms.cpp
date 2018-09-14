#include "generator/generator_tests_support/test_with_custom_mwms.hpp"

#include "platform/local_country_file_utils.hpp"

#include "base/stl_helpers.hpp"

namespace generator
{
namespace tests_support
{
TestWithCustomMwms::~TestWithCustomMwms()
{
  for (auto const & file : m_files)
    Cleanup(file);
}

// static
void TestWithCustomMwms::Cleanup(platform::LocalCountryFile const & file)
{
  platform::CountryIndexes::DeleteFromDisk(file);
  file.DeleteFromDisk(MapOptions::Map);
}
}  // namespace tests_support
}  // namespace generator
