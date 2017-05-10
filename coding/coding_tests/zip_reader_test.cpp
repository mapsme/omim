#include "testing/testing.hpp"

#include "coding/zip_reader.hpp"
#include "coding/file_writer.hpp"

#include "base/logging.hpp"
#include "base/macros.hpp"

static char const zipBytes[] = "PK\003\004\n\0\0\0\0\0\222\226\342>\302\032"
"x\372\005\0\0\0\005\0\0\0\b\0\034\0te"
"st.txtUT\t\0\003\303>\017N\017"
"?\017Nux\v\0\001\004\365\001\0\0\004P\0"
"\0\0Test\nPK\001\002\036\003\n\0\0"
"\0\0\0\222\226\342>\302\032x\372\005\0\0\0\005"
"\0\0\0\b\0\030\0\0\0\0\0\0\0\0\0\244"
"\201\0\0\0\0test.txtUT\005"
"\0\003\303>\017Nux\v\0\001\004\365\001\0\0"
"\004P\0\0\0PK\005\006\0\0\0\0\001\0\001"
"\0N\0\0\0G\0\0\0\0\0";

UNIT_TEST(ZipReaderSmoke)
{
  std::string const ZIPFILE = "smoke_test.zip";
  {
    FileWriter f(ZIPFILE);
    f.Write(zipBytes, ARRAY_SIZE(zipBytes) - 1);
  }

  bool noException = true;
  try
  {
    ZipFileReader r(ZIPFILE, "test.txt");
    std::string s;
    r.ReadAsString(s);
    TEST_EQUAL(s, "Test\n", ("Invalid zip file contents"));
  }
  catch (std::exception const & e)
  {
    noException = false;
    LOG(LERROR, (e.what()));
  }
  TEST(noException, ("Unhandled exception"));

  // invalid zip
  noException = true;
  try
  {
    ZipFileReader r("some_nonexisting_filename", "test.txt");
  }
  catch (std::exception const &)
  {
    noException = false;
  }
  TEST(!noException, ());

  // invalid file inside zip
  noException = true;
  try
  {
    ZipFileReader r(ZIPFILE, "test");
  }
  catch (std::exception const &)
  {
    noException = false;
  }
  TEST(!noException, ());

  FileWriter::DeleteFileX(ZIPFILE);
}

/// zip file with 3 files inside: 1.txt, 2.txt, 3.ttt
static char const zipBytes2[] = "\x50\x4b\x3\x4\xa\x0\x0\x0\x0\x0\x92\x6b\xf6\x3e\x53\xfc\x51\x67\x2\x0\x0"
"\x0\x2\x0\x0\x0\x5\x0\x1c\x0\x31\x2e\x74\x78\x74\x55\x54\x9\x0\x3\xd3\x50\x29\x4e\xd4\x50\x29\x4e\x75\x78"
"\xb\x0\x1\x4\xf5\x1\x0\x0\x4\x14\x0\x0\x0\x31\xa\x50\x4b\x3\x4\xa\x0\x0\x0\x0\x0\x95\x6b\xf6\x3e\x90\xaf"
"\x7c\x4c\x2\x0\x0\x0\x2\x0\x0\x0\x5\x0\x1c\x0\x32\x2e\x74\x78\x74\x55\x54\x9\x0\x3\xd9\x50\x29\x4e\xd9\x50"
"\x29\x4e\x75\x78\xb\x0\x1\x4\xf5\x1\x0\x0\x4\x14\x0\x0\x0\x32\xa\x50\x4b\x3\x4\xa\x0\x0\x0\x0\x0\x9c\x6b"
"\xf6\x3e\xd1\x9e\x67\x55\x2\x0\x0\x0\x2\x0\x0\x0\x5\x0\x1c\x0\x33\x2e\x74\x74\x74\x55\x54\x9\x0\x3\xe8\x50"
"\x29\x4e\xe9\x50\x29\x4e\x75\x78\xb\x0\x1\x4\xf5\x1\x0\x0\x4\x14\x0\x0\x0\x33\xa\x50\x4b\x1\x2\x1e\x3\xa"
"\x0\x0\x0\x0\x0\x92\x6b\xf6\x3e\x53\xfc\x51\x67\x2\x0\x0\x0\x2\x0\x0\x0\x5\x0\x18\x0\x0\x0\x0\x0\x1\x0\x0"
"\x0\xa4\x81\x0\x0\x0\x0\x31\x2e\x74\x78\x74\x55\x54\x5\x0\x3\xd3\x50\x29\x4e\x75\x78\xb\x0\x1\x4\xf5\x1\x0"
"\x0\x4\x14\x0\x0\x0\x50\x4b\x1\x2\x1e\x3\xa\x0\x0\x0\x0\x0\x95\x6b\xf6\x3e\x90\xaf\x7c\x4c\x2\x0\x0\x0\x2"
"\x0\x0\x0\x5\x0\x18\x0\x0\x0\x0\x0\x1\x0\x0\x0\xa4\x81\x41\x0\x0\x0\x32\x2e\x74\x78\x74\x55\x54\x5\x0\x3"
"\xd9\x50\x29\x4e\x75\x78\xb\x0\x1\x4\xf5\x1\x0\x0\x4\x14\x0\x0\x0\x50\x4b\x1\x2\x1e\x3\xa\x0\x0\x0\x0\x0"
"\x9c\x6b\xf6\x3e\xd1\x9e\x67\x55\x2\x0\x0\x0\x2\x0\x0\x0\x5\x0\x18\x0\x0\x0\x0\x0\x1\x0\x0\x0\xa4\x81\x82"
"\x0\x0\x0\x33\x2e\x74\x74\x74\x55\x54\x5\x0\x3\xe8\x50\x29\x4e\x75\x78\xb\x0\x1\x4\xf5\x1\x0\x0\x4\x14\x0"
"\x0\x0\x50\x4b\x5\x6\x0\x0\x0\x0\x3\x0\x3\x0\xe1\x0\x0\x0\xc3\x0\x0\x0\x0\x0";

static char const invalidZip[] = "1234567890asdqwetwezxvcbdhg322353tgfsd";

UNIT_TEST(ZipFilesList)
{
  std::string const ZIPFILE = "list_test.zip";
  {
    FileWriter f(ZIPFILE);
    f.Write(zipBytes2, ARRAY_SIZE(zipBytes2) - 1);
  }
  TEST(ZipFileReader::IsZip(ZIPFILE), ());
  std::string const ZIPFILE_INVALID = "invalid_test.zip";
  {
    FileWriter f(ZIPFILE_INVALID);
    f.Write(invalidZip, ARRAY_SIZE(invalidZip) - 1);
  }
  TEST(!ZipFileReader::IsZip(ZIPFILE_INVALID), ());

  try
  {
    ZipFileReader::FileListT files;
    ZipFileReader::FilesList(ZIPFILE, files);

    TEST_EQUAL(files.size(), 3, ());
    TEST_EQUAL(files[0].first, "1.txt", ());
    TEST_EQUAL(files[0].second, 2, ());
    TEST_EQUAL(files[1].first, "2.txt", ());
    TEST_EQUAL(files[1].second, 2, ());
    TEST_EQUAL(files[2].first, "3.ttt", ());
    TEST_EQUAL(files[2].second, 2, ());
  }
  catch (std::exception const & e)
  {
    TEST(false, ("Can't get list of files inside zip", e.what()));
  }

  try
  {
    ZipFileReader::FileListT files;
    ZipFileReader::FilesList(ZIPFILE_INVALID, files);
    TEST(false, ("This test shouldn't be reached - exception should be thrown"));
  }
  catch (std::exception const &)
  {
  }

  FileWriter::DeleteFileX(ZIPFILE_INVALID);
  FileWriter::DeleteFileX(ZIPFILE);
}

/// Compressed zip file with 2 files in assets folder:
/// assets/aaaaaaaaaa.txt (contains text "aaaaaaaaaa\x0A")
/// assets/holalala.txt (contains text "Holalala\x0A")
static char const zipBytes3[] = \
	"\x50\x4B\x03\x04\x14\x00\x02\x00\x08\x00\xAF\x96\x56\x40\x42\xE5\x26\x8F\x06\x00" \
	"\x00\x00\x0B\x00\x00\x00\x15\x00\x1C\x00\x61\x73\x73\x65\x74\x73\x2F\x61\x61\x61" \
	"\x61\x61\x61\x61\x61\x61\x61\x2E\x74\x78\x74\x55\x54\x09\x00\x03\x7A\x0F\x45\x4F" \
	"\xD8\x0F\x45\x4F\x75\x78\x0B\x00\x01\x04\xF5\x01\x00\x00\x04\x14\x00\x00\x00\x4B" \
	"\x4C\x84\x01\x2E\x00\x50\x4B\x03\x04\x14\x00\x02\x00\x08\x00\xE6\x96\x56\x40\x5E" \
	"\x76\x90\x07\x08\x00\x00\x00\x09\x00\x00\x00\x13\x00\x1C\x00\x61\x73\x73\x65\x74" \
	"\x73\x2F\x68\x6F\x6C\x61\x6C\x61\x6C\x61\x2E\x74\x78\x74\x55\x54\x09\x00\x03\xDF" \
	"\x0F\x45\x4F\xDC\x0F\x45\x4F\x75\x78\x0B\x00\x01\x04\xF5\x01\x00\x00\x04\x14\x00" \
	"\x00\x00\xF3\xC8\xCF\x49\x04\x41\x2E\x00\x50\x4B\x01\x02\x1E\x03\x14\x00\x02\x00" \
	"\x08\x00\xAF\x96\x56\x40\x42\xE5\x26\x8F\x06\x00\x00\x00\x0B\x00\x00\x00\x15\x00" \
	"\x18\x00\x00\x00\x00\x00\x01\x00\x00\x00\xA4\x81\x00\x00\x00\x00\x61\x73\x73\x65" \
	"\x74\x73\x2F\x61\x61\x61\x61\x61\x61\x61\x61\x61\x61\x2E\x74\x78\x74\x55\x54\x05" \
	"\x00\x03\x7A\x0F\x45\x4F\x75\x78\x0B\x00\x01\x04\xF5\x01\x00\x00\x04\x14\x00\x00" \
	"\x00\x50\x4B\x01\x02\x1E\x03\x14\x00\x02\x00\x08\x00\xE6\x96\x56\x40\x5E\x76\x90" \
	"\x07\x08\x00\x00\x00\x09\x00\x00\x00\x13\x00\x18\x00\x00\x00\x00\x00\x01\x00\x00" \
	"\x00\xA4\x81\x55\x00\x00\x00\x61\x73\x73\x65\x74\x73\x2F\x68\x6F\x6C\x61\x6C\x61" \
	"\x6C\x61\x2E\x74\x78\x74\x55\x54\x05\x00\x03\xDF\x0F\x45\x4F\x75\x78\x0B\x00\x01" \
	"\x04\xF5\x01\x00\x00\x04\x14\x00\x00\x00\x50\x4B\x05\x06\x00\x00\x00\x00\x02\x00" \
	"\x02\x00\xB4\x00\x00\x00\xAA\x00\x00\x00\x00\x00";

UNIT_TEST(ZipExtract)
{
  std::string const ZIPFILE = "test.zip";
  {
    FileWriter f(ZIPFILE);
    f.Write(zipBytes3, ARRAY_SIZE(zipBytes3));
  }
  TEST(ZipFileReader::IsZip(ZIPFILE), ("Not a zip file"));

  ZipFileReader::FileListT files;
  ZipFileReader::FilesList(ZIPFILE, files);
  TEST_EQUAL(files.size(), 2, ());

  std::string const OUTFILE = "out.tmp";
  std::string s;
  ZipFileReader::UnzipFile(ZIPFILE, files[0].first, OUTFILE);
  {
    FileReader(OUTFILE).ReadAsString(s);
  }
  TEST_EQUAL(s, "aaaaaaaaaa\x0A", ());
  // OUTFILE should be rewritten correctly in the next lines
  ZipFileReader::UnzipFile(ZIPFILE, files[1].first, OUTFILE);
  {
    FileReader(OUTFILE).ReadAsString(s);
  }
  TEST_EQUAL(s, "Holalala\x0A", ());
  FileWriter::DeleteFileX(OUTFILE);

  FileWriter::DeleteFileX(ZIPFILE);
}

UNIT_TEST(ZipFileSizes)
{
  std::string const ZIPFILE = "test.zip";
  {
    FileWriter f(ZIPFILE);
    f.Write(zipBytes3, ARRAY_SIZE(zipBytes3));
  }
  TEST(ZipFileReader::IsZip(ZIPFILE), ("Not a zip file"));

  ZipFileReader::FileListT files;
  ZipFileReader::FilesList(ZIPFILE, files);
  TEST_EQUAL(files.size(), 2, ());

  {
    ZipFileReader file(ZIPFILE, files[0].first);
    TEST_EQUAL(file.Size(), 6, ());
    TEST_EQUAL(file.UncompressedSize(), 11, ());
  }

  {
    ZipFileReader file(ZIPFILE, files[1].first);
    TEST_EQUAL(file.Size(), 8, ());
    TEST_EQUAL(file.UncompressedSize(), 9, ());
  }

  FileWriter::DeleteFileX(ZIPFILE);
}
