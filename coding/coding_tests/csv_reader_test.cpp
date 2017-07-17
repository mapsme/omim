#include "testing/testing.hpp"

#include "coding/csv_file_reader.hpp"

#include "platform/platform_tests_support/scoped_file.hpp"

#include <string>
#include <vector>

namespace
{
std::string const kCSV1 = "a,b,c,d\ne,f,g h";
std::string const kCSV2 = "a,b,cd a b, c";
std::string const kCSV3 = "";
}  // namespace

using coding::CSVReader;
using Row = std::vector<std::string>;
using File = std::vector<Row>;

UNIT_TEST(CSVReaderSmoke)
{
  auto const fileName = "test.csv";
  platform::tests_support::ScopedFile sf(fileName, kCSV1);
  auto const & filePath = sf.GetFullPath();

  CSVReader reader;
  reader.ReadFullFile(filePath, [](File const & file) {
    TEST_EQUAL(file.size(), 1, ());
    TEST_EQUAL(file[0].size(), 3, ());
    Row const firstRow = {"e", "f", "g h"};
    TEST_EQUAL(file[0], firstRow, ());
  });

  CSVReader::Params p;
  p.m_shouldReadHeader = true;
  reader.ReadFullFile(filePath,
                      [](File const & file) {
                        TEST_EQUAL(file.size(), 2, ());
                        Row const headerRow = {"a", "b", "c", "d"};
                        TEST_EQUAL(file[0], headerRow, ());
                      },
                      p);
}

UNIT_TEST(CSVReaderCustomDelimiter)
{
  auto const fileName = "test.csv";
  platform::tests_support::ScopedFile sf(fileName, kCSV2);
  auto const & filePath = sf.GetFullPath();

  CSVReader reader;
  CSVReader::Params p;
  p.m_shouldReadHeader = true;
  p.m_delimiter = ' ';

  reader.ReadLineByLine(filePath,
                        [](Row const & row) {
                          Row const firstRow = {"a,b,cd", "a", "b,", "c"};
                          TEST_EQUAL(row, firstRow, ());
                        },
                        p);
}

UNIT_TEST(CSVReaderEmptyFile)
{
  auto const fileName = "test.csv";
  platform::tests_support::ScopedFile sf(fileName, kCSV2);
  auto const & filePath = sf.GetFullPath();

  CSVReader reader;
  reader.ReadFullFile(filePath, [](File const & file) { TEST_EQUAL(file.size(), 0, ()); });
}
