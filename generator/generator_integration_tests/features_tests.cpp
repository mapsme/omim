#include "testing/testing.hpp"

#include "generator/generator_integration_tests/helpers.hpp"

#include "generator/feature_builder.hpp"
#include "generator/filter_world.hpp"
#include "generator/generate_info.hpp"
#include "generator/raw_generator.hpp"

#include "indexer/classificator.hpp"
#include "indexer/classificator_loader.hpp"
#include "indexer/ftypes_matcher.hpp"

#include "platform/platform_tests_support/helpers.hpp"

#include "platform/platform.hpp"

#include "base/assert.hpp"
#include "base/file_name_utils.hpp"
#include "base/scope_guard.hpp"

#include "defines.hpp"

#include <cstdio>
#include <sstream>
#include <string>

using namespace generator_integration_tests;

struct CountryFeaturesCounters
{
  int64_t m_fbs = 0;
  int64_t m_geometryPoints = 0;
  int64_t m_point = 0;
  int64_t m_line = 0;
  int64_t m_area = 0;
  int64_t m_poi = 0;
  int64_t m_cityTownOrVillage = 0;
  int64_t m_bookingHotels = 0;

  CountryFeaturesCounters() = default;

  constexpr CountryFeaturesCounters(int64_t fbs, int64_t geometryPoints, int64_t point, int64_t line,
                                    int64_t area, int64_t poi, int64_t cityTownOrVillage,
                                    int64_t bookingHotels)
    : m_fbs(fbs)
    , m_geometryPoints(geometryPoints)
    , m_point(point)
    , m_line(line)
    , m_area(area)
    , m_poi(poi)
    , m_cityTownOrVillage(cityTownOrVillage)
    , m_bookingHotels(bookingHotels)
  {
  }

  CountryFeaturesCounters operator+(CountryFeaturesCounters const & rhs) const
  {
    return CountryFeaturesCounters(m_fbs + rhs.m_fbs, m_geometryPoints + rhs.m_geometryPoints,
                                   m_point + rhs.m_point, m_line + rhs.m_line, m_area + rhs.m_area,
                                   m_poi + rhs.m_poi, m_cityTownOrVillage + rhs.m_cityTownOrVillage,
                                   m_bookingHotels + rhs.m_bookingHotels);
  }

  CountryFeaturesCounters operator-(CountryFeaturesCounters const & rhs) const
  {
    return CountryFeaturesCounters(m_fbs - rhs.m_fbs, m_geometryPoints - rhs.m_geometryPoints,
                                   m_point - rhs.m_point, m_line - rhs.m_line, m_area - rhs.m_area,
                                   m_poi - rhs.m_poi, m_cityTownOrVillage - rhs.m_cityTownOrVillage,
                                   m_bookingHotels - rhs.m_bookingHotels);
  }

  bool operator==(CountryFeaturesCounters const & rhs) const
  {
    return m_fbs == rhs.m_fbs && m_geometryPoints == rhs.m_geometryPoints &&
           m_point == rhs.m_point && m_line == rhs.m_line && m_area == rhs.m_area &&
           m_poi == rhs.m_poi && m_cityTownOrVillage == rhs.m_cityTownOrVillage &&
           m_bookingHotels == rhs.m_bookingHotels;
  }

  bool operator!=(CountryFeaturesCounters const & rhs) const { return !(*this == rhs); }
};

struct CountryFeatureResults
{
  CountryFeatureResults() = default;
  CountryFeatureResults(CountryFeaturesCounters actual, CountryFeaturesCounters expected)
    : m_actual(actual), m_expected(expected)
  {
  }

  CountryFeaturesCounters m_actual;
  CountryFeaturesCounters m_expected;
};

void TestAndLogCountryFeatures(std::map<std::string, CountryFeatureResults> const & results)
{
  for (auto const & result : results)
  {
    if (result.second.m_actual != result.second.m_expected)
    {
      LOG(LINFO, ("Unexpectad result for", result.first, "actual:", result.second.m_actual,
                  "expected:", result.second.m_expected,
                  "the difference is:", result.second.m_actual - result.second.m_expected));
    }
  }

  for (auto const & result : results)
  {
    TEST_EQUAL(result.second.m_actual, result.second.m_expected,
               (result.first, "difference:", result.second.m_actual - result.second.m_expected));
  }
}

std::string DebugPrint(CountryFeaturesCounters const & cnt)
{
  std::ostringstream out;
  out << "CountryFeaturesCount(fbs = " << cnt.m_fbs << ", geometryPoints = " << cnt.m_geometryPoints
      << ", point = " << cnt.m_point << ", line = " << cnt.m_line << ", area = " << cnt.m_area
      << ", poi = " << cnt.m_poi << ", cityTownOrVillage = " << cnt.m_cityTownOrVillage
      << ", bookingHotels = " << cnt.m_bookingHotels << ")";
  return out.str();
}

CountryFeaturesCounters constexpr kWorldCounters(945 /* fbs */, 364406 /* geometryPoints */,
                                                 334 /* point */, 598 /* line */, 13 /* area */,
                                                 428 /* poi */, 172 /* cityTownOrVillage */,
                                                 0 /* bookingHotels */);

CountryFeaturesCounters constexpr kNorthAucklandCounters(
    1812333 /* fbs */, 12196704 /* geometryPoints */, 1007584 /* point */, 205634 /* line */,
    599115 /* area */, 212614 /* poi */, 521 /* cityTownOrVillage */, 3557 /* bookingHotels */);

CountryFeaturesCounters constexpr kNorthWellingtonCounters(
    797846 /* fbs */, 7771680 /* geometryPoints */, 460559 /* point */, 87011 /* line */,
    250276 /* area */, 95897 /* poi */, 297 /* cityTownOrVillage */, 1062 /* bookingHotels */);

CountryFeaturesCounters constexpr kSouthCanterburyCounters(
    637244 /* fbs */, 6984549 /* geometryPoints */, 397961 /* point */, 81697 /* line */,
    157586 /* area */, 89700 /* poi */, 331 /* cityTownOrVillage */, 2085 /* bookingHotels */);

CountryFeaturesCounters constexpr kSouthSouthlandCounters(
    340637 /* fbs */, 5342359 /* geometryPoints */, 185994 /* point */, 40117 /* line */,
    114526 /* area */, 40672 /* poi */, 297 /* cityTownOrVillage */, 1621 /* bookingHotels */);

CountryFeaturesCounters constexpr kSouthSouthlandMixedNodesCounters(
    2 /* fbs */, 2 /* geometryPoints */, 2 /* point */, 0 /* line */, 0 /* area */, 0 /* poi */,
    0 /* cityTownOrVillage */, 0 /* bookingHotels */);

CountryFeaturesCounters constexpr kNorthAucklandComplexFeaturesCounters(
    288 /* fbs */, 16119 /* geometryPoints */, 0 /* point */, 252 /* line */, 36 /* area */,
    0 /* poi */, 0 /* cityTownOrVillage */, 0 /* bookingHotels */);

CountryFeaturesCounters constexpr kNorthWellingtonComplexFeaturesCounters(
    254 /* fbs */, 18434 /* geometryPoints */, 0 /* point */, 244 /* line */, 10 /* area */,
    0 /* poi */, 0 /* cityTownOrVillage */, 0 /* bookingHotels */);

CountryFeaturesCounters constexpr kSouthCanterburyComplexFeaturesCounters(
    1037 /* fbs */, 73854 /* geometryPoints */, 0 /* point */, 1016 /* line */, 21 /* area */,
    0 /* poi */, 0 /* cityTownOrVillage */, 0 /* bookingHotels */);

CountryFeaturesCounters constexpr kSouthSouthlandComplexFeaturesCounters(
    1252 /* fbs */, 141706 /* geometryPoints */, 0 /* point */, 1245 /* line */, 7 /* area */,
    0 /* poi */, 0 /* cityTownOrVillage */, 0 /* bookingHotels */);

class FeatureIntegrationTests
{
public:
  FeatureIntegrationTests()
  {
    // You can get features-2019_07_17__13_39_20 by running:
    // rsync -v -p testdata.mapsme.cloud.devmail.ru::testdata/features-2019_07_17__13_39_20.zip .
    Init("features-2019_07_17__13_39_20" /* archiveName */);
    size_t const kMaxOpenFiles = 4096;
    platform::tests_support::ChangeMaxNumberOfOpenFiles(kMaxOpenFiles);
  }

  ~FeatureIntegrationTests()
  {
    CHECK(Platform::RmDirRecursively(m_testPath), ());
    CHECK(Platform::RemoveFileIfExists(m_mixedNodesFilenames.first), ());
    CHECK(Platform::RemoveFileIfExists(m_mixedTagsFilenames.first), ());
    CHECK_EQUAL(
        std::rename(m_mixedNodesFilenames.second.c_str(), m_mixedNodesFilenames.first.c_str()), 0,
        ());
    CHECK_EQUAL(
        std::rename(m_mixedTagsFilenames.second.c_str(), m_mixedTagsFilenames.first.c_str()), 0,
        ());
  }

  void BuildCoasts()
  {
    auto const worldCoastsGeom = m_genInfo.GetIntermediateFileName(WORLD_COASTS_FILE_NAME ".geom");
    auto const worldCoastsRawGeom =
        m_genInfo.GetIntermediateFileName(WORLD_COASTS_FILE_NAME ".rawgeom");

    CHECK(!Platform::IsFileExistsByFullPath(worldCoastsGeom), ());
    CHECK(!Platform::IsFileExistsByFullPath(worldCoastsRawGeom), ());

    generator::RawGenerator rawGenerator(m_genInfo, m_threadCount);
    rawGenerator.GenerateCoasts();
    TEST(rawGenerator.Execute(), ());

    TEST(Platform::IsFileExistsByFullPath(worldCoastsGeom), ());
    TEST(Platform::IsFileExistsByFullPath(worldCoastsRawGeom), ());
    uint64_t fileSize = 0;
    CHECK(Platform::GetFileSizeByFullPath(worldCoastsGeom, fileSize), ());
    TEST_GREATER(fileSize, 0, ());
    CHECK(Platform::GetFileSizeByFullPath(worldCoastsRawGeom, fileSize), ());
    TEST_GREATER(fileSize, 0, ());

    auto const fbs = feature::ReadAllDatRawFormat(worldCoastsGeom);
    size_t geomeriesCnt = 0;
    size_t geometryPointsCnt = 0;
    for (auto const & fb : fbs)
    {
      geomeriesCnt += fb.GetGeometry().size();
      geometryPointsCnt += fb.GetPointsCount();
    }

    TEST_EQUAL(fbs.size(), 340, ());
    TEST_EQUAL(geomeriesCnt, 6814, ());
    TEST_EQUAL(geometryPointsCnt, 512102, ());
  }

  void BuildWorld()
  {
    auto const world = m_genInfo.GetTmpFileName(WORLD_FILE_NAME);
    CHECK(!Platform::IsFileExistsByFullPath(world), ());

    generator::RawGenerator rawGenerator(m_genInfo, m_threadCount);
    rawGenerator.GenerateCoasts();
    rawGenerator.GenerateWorld();
    rawGenerator.GenerateCountries();
    TEST(rawGenerator.Execute(), ());

    TEST(Platform::IsFileExistsByFullPath(world), ());

    auto const actual = GetCountersForCountry(world);
    TEST_EQUAL(actual, kWorldCounters, ("kWorldCounters difference:", actual - kWorldCounters));
  }

  void BuildCountries()
  {
    m_genInfo.m_emitCoasts = true;
    m_genInfo.m_citiesBoundariesFilename =
        m_genInfo.GetIntermediateFileName("citiesboundaries.bin");
    m_genInfo.m_bookingDataFilename = m_genInfo.GetIntermediateFileName("hotels.csv");

    auto const northAuckland = m_genInfo.GetTmpFileName("New Zealand North_Auckland");
    auto const northWellington = m_genInfo.GetTmpFileName("New Zealand North_Wellington");
    auto const southCanterbury = m_genInfo.GetTmpFileName("New Zealand South_Canterbury");
    auto const southSouthland = m_genInfo.GetTmpFileName("New Zealand South_Southland");
    for (auto const & mwmTmp : {northAuckland, northWellington, southCanterbury, southSouthland})
      CHECK(!Platform::IsFileExistsByFullPath(mwmTmp), (mwmTmp));

    generator::RawGenerator rawGenerator(m_genInfo, m_threadCount);
    rawGenerator.GenerateCoasts();
    rawGenerator.GenerateCountries();
    TEST(rawGenerator.Execute(), ());

    std::map<std::string, CountryFeatureResults> results;
    results["kNorthAucklandCounters"] =
        CountryFeatureResults(GetCountersForCountry(northAuckland), kNorthAucklandCounters);
    results["kNorthWellingtonCounters"] =
        CountryFeatureResults(GetCountersForCountry(northWellington), kNorthWellingtonCounters);
    results["kSouthCanterburyCounters"] =
        CountryFeatureResults(GetCountersForCountry(southCanterbury), kSouthCanterburyCounters);
    results["kSouthSouthlandCounters"] =
        CountryFeatureResults(GetCountersForCountry(southSouthland), kSouthSouthlandCounters);
    TestAndLogCountryFeatures(results);
  }

  void BuildCountriesWithComplex()
  {
    m_genInfo.m_emitCoasts = true;
    m_genInfo.m_citiesBoundariesFilename =
        m_genInfo.GetIntermediateFileName("citiesboundaries.bin");
    m_genInfo.m_bookingDataFilename = m_genInfo.GetIntermediateFileName("hotels.csv");
    m_genInfo.m_complexHierarchyFilename = base::JoinPath(m_testPath, "hierarchy.csv");

    auto const northAuckland = m_genInfo.GetTmpFileName("New Zealand North_Auckland");
    auto const northWellington = m_genInfo.GetTmpFileName("New Zealand North_Wellington");
    auto const southCanterbury = m_genInfo.GetTmpFileName("New Zealand South_Canterbury");
    auto const southSouthland = m_genInfo.GetTmpFileName("New Zealand South_Southland");
    for (auto const & mwmTmp : {northAuckland, northWellington, southCanterbury, southSouthland})
      CHECK(!Platform::IsFileExistsByFullPath(mwmTmp), (mwmTmp));

    generator::RawGenerator rawGenerator(m_genInfo, m_threadCount);
    rawGenerator.GenerateCoasts();
    rawGenerator.GenerateCountries();
    TEST(rawGenerator.Execute(), ());

    std::map<std::string, CountryFeatureResults> results;
    results["kNorthAucklandCounters + kNorthAucklandComplexFeaturesCounters"] =
        CountryFeatureResults(GetCountersForCountry(northAuckland),
                              kNorthAucklandCounters + kNorthAucklandComplexFeaturesCounters);
    results["kNorthWellingtonCounters + kNorthWellingtonComplexFeaturesCounters"] =
        CountryFeatureResults(GetCountersForCountry(northWellington),
                              kNorthWellingtonCounters + kNorthWellingtonComplexFeaturesCounters);
    results["kSouthCanterburyCounters + kSouthCanterburyComplexFeaturesCounters"] =
        CountryFeatureResults(GetCountersForCountry(southCanterbury),
                              kSouthCanterburyCounters + kSouthCanterburyComplexFeaturesCounters);
    results["kSouthSouthlandCounters + kSouthSouthlandComplexFeaturesCounters"] =
        CountryFeatureResults(GetCountersForCountry(southSouthland),
                              kSouthSouthlandCounters + kSouthSouthlandComplexFeaturesCounters);
    TestAndLogCountryFeatures(results);
  }

  void CheckMixedTagsAndNodes()
  {
    m_genInfo.m_emitCoasts = true;
    m_genInfo.m_citiesBoundariesFilename =
        m_genInfo.GetIntermediateFileName("citiesboundaries.bin");
    m_genInfo.m_bookingDataFilename = m_genInfo.GetIntermediateFileName("hotels.csv");

    auto const northAuckland = m_genInfo.GetTmpFileName("New Zealand North_Auckland");
    auto const northWellington = m_genInfo.GetTmpFileName("New Zealand North_Wellington");
    auto const southCanterbury = m_genInfo.GetTmpFileName("New Zealand South_Canterbury");
    auto const southSouthland = m_genInfo.GetTmpFileName("New Zealand South_Southland");
    auto const world = m_genInfo.GetTmpFileName("World");
    auto const counties = {northAuckland, northWellington, southCanterbury, southSouthland, world};
    for (auto const & mwmTmp : counties)
      CHECK(!Platform::IsFileExistsByFullPath(mwmTmp), (mwmTmp));

    generator::RawGenerator rawGenerator(m_genInfo, m_threadCount);
    rawGenerator.GenerateCoasts();
    rawGenerator.GenerateCountries(true /* needMixTagsAndNodes */);
    rawGenerator.GenerateWorld(true /* needMixTags */);
    TEST(rawGenerator.Execute(), ());

    size_t partner1CntReal = 0;

    std::map<std::string, CountryFeatureResults> results;
    results["kNorthAucklandCounters"] =
        CountryFeatureResults(GetCountersForCountry(northAuckland), kNorthAucklandCounters);
    results["kNorthWellingtonCounters"] =
        CountryFeatureResults(GetCountersForCountry(northWellington), kNorthWellingtonCounters);
    results["kSouthCanterburyCounters"] =
        CountryFeatureResults(GetCountersForCountry(southCanterbury), kSouthCanterburyCounters);
    results["kSouthSouthlandCounters + kSouthSouthlandMixedNodesCounters"] = CountryFeatureResults(
        GetCountersForCountry(
            southSouthland,
            [&](auto const & fb) {
              static auto const partner1 = classif().GetTypeByPath({"sponsored", "partner1"});
              if (fb.HasType(partner1))
                ++partner1CntReal;
            }),
        kSouthSouthlandCounters + kSouthSouthlandMixedNodesCounters);
    results["kWorldCounters"] = CountryFeatureResults(GetCountersForCountry(world), kWorldCounters);

    TestAndLogCountryFeatures(results);
    TEST_EQUAL(partner1CntReal, 4, ());
  }

  void CheckGeneratedData()
  {
    m_genInfo.m_emitCoasts = true;
    m_genInfo.m_citiesBoundariesFilename =
        m_genInfo.GetIntermediateFileName("citiesboundaries.bin");
    auto const cameraToWays = m_genInfo.GetIntermediateFileName(CAMERAS_TO_WAYS_FILENAME);
    auto const citiesAreas = m_genInfo.GetIntermediateFileName(CITIES_AREAS_TMP_FILENAME);
    auto const maxSpeeds = m_genInfo.GetIntermediateFileName(MAXSPEEDS_FILENAME);
    auto const metalines = m_genInfo.GetIntermediateFileName(METALINES_FILENAME);
    auto const restrictions = m_genInfo.GetIntermediateFileName(RESTRICTIONS_FILENAME);
    auto const roadAccess = m_genInfo.GetIntermediateFileName(ROAD_ACCESS_FILENAME);

    for (auto const & generatedFile :
         {cameraToWays, citiesAreas, maxSpeeds, metalines, restrictions, roadAccess,
          m_genInfo.m_citiesBoundariesFilename})
    {
      CHECK(!Platform::IsFileExistsByFullPath(generatedFile), (generatedFile));
    }

    generator::RawGenerator rawGenerator(m_genInfo, m_threadCount);
    rawGenerator.GenerateCoasts();
    rawGenerator.GenerateCountries();
    TEST(rawGenerator.Execute(), ());

    TestGeneratedFile(cameraToWays, 0 /* fileSize */);
    TestGeneratedFile(citiesAreas, 18601 /* fileSize */);
    TestGeneratedFile(maxSpeeds, 1301515 /* fileSize */);
    TestGeneratedFile(metalines, 288032 /* fileSize */);
    TestGeneratedFile(restrictions, 371110 /* fileSize */);
    TestGeneratedFile(roadAccess, 1915402 /* fileSize */);
    TestGeneratedFile(m_genInfo.m_citiesBoundariesFilename, 95 /* fileSize */);
  }

  void BuildWorldOneThread()
  {
    m_threadCount = 1;
    BuildWorld();
  }

  void BuildCountriesOneThread()
  {
    m_threadCount = 1;
    BuildCountries();
  }

  void CheckGeneratedDataOneThread()
  {
    m_threadCount = 1;
    CheckGeneratedData();
  }

private:
  CountryFeaturesCounters GetCountersForCountry(
      std::string const & path, std::function<void(feature::FeatureBuilder const &)> const & fn =
                                    [](feature::FeatureBuilder const &) {})
  {
    CHECK(Platform::IsFileExistsByFullPath(path), ());
    auto const fbs = feature::ReadAllDatRawFormat(path);
    CountryFeaturesCounters actual;
    actual.m_fbs = fbs.size();
    for (auto const & fb : fbs)
    {
      actual.m_geometryPoints += fb.IsPoint() ? 1 : fb.GetPointsCount();
      if (fb.IsPoint())
        ++actual.m_point;
      else if (fb.IsLine())
        ++actual.m_line;
      else if (fb.IsArea())
        ++actual.m_area;

      auto static const & poiChecker = ftypes::IsPoiChecker::Instance();
      if (poiChecker(fb.GetTypes()))
        ++actual.m_poi;

      auto const & isCityTownOrVillage = ftypes::IsCityTownOrVillageChecker::Instance();
      if (isCityTownOrVillage(fb.GetTypes()))
        ++actual.m_cityTownOrVillage;

      auto static const & bookingChecker = ftypes::IsBookingHotelChecker::Instance();
      if (bookingChecker(fb.GetTypes()))
        ++actual.m_bookingHotels;

      fn(fb);
    }

    return actual;
  }

  void TestGeneratedFile(std::string const & path, size_t fileSize)
  {
    TEST(Platform::IsFileExistsByFullPath(path), (path));
    uint64_t fileSizeReal = 0;
    CHECK(Platform::GetFileSizeByFullPath(path, fileSizeReal), (path));
    TEST_EQUAL(fileSizeReal, fileSize, (path));
  }

  void Init(std::string const & archiveName)
  {
    classificator::Load();
    auto const & options = GetTestingOptions();
    auto & platform = GetPlatform();
    platform.SetResourceDir(options.m_resourcePath);
    platform.SetSettingsDir(options.m_resourcePath);
    m_threadCount = static_cast<size_t>(platform.CpuCores());
    m_testPath = base::JoinPath(platform.WritableDir(), "gen-test");
    m_genInfo.SetNodeStorageType("map");
    m_genInfo.SetOsmFileType("o5m");
    m_genInfo.m_intermediateDir = base::JoinPath(m_testPath, archiveName, "intermediate_data");
    m_genInfo.m_targetDir = m_genInfo.m_intermediateDir;
    m_genInfo.m_tmpDir = base::JoinPath(m_genInfo.m_intermediateDir, "tmp");
    m_genInfo.m_osmFileName = base::JoinPath(m_testPath, "planet.o5m");
    m_genInfo.m_popularPlacesFilename = m_genInfo.GetIntermediateFileName("popular_places.csv");
    m_genInfo.m_idToWikidataFilename = m_genInfo.GetIntermediateFileName("wiki_urls.csv");
    DecompressZipArchive(base::JoinPath(options.m_dataPath, archiveName + ".zip"), m_testPath);

    m_mixedNodesFilenames.first = base::JoinPath(platform.ResourcesDir(), MIXED_NODES_FILE);
    m_mixedNodesFilenames.second = base::JoinPath(platform.ResourcesDir(), MIXED_NODES_FILE "_");
    m_mixedTagsFilenames.first = base::JoinPath(platform.ResourcesDir(), MIXED_TAGS_FILE);
    m_mixedTagsFilenames.second = base::JoinPath(platform.ResourcesDir(), MIXED_TAGS_FILE "_");

    CHECK_EQUAL(
        std::rename(m_mixedNodesFilenames.first.c_str(), m_mixedNodesFilenames.second.c_str()), 0,
        ());
    CHECK_EQUAL(
        std::rename(m_mixedTagsFilenames.first.c_str(), m_mixedTagsFilenames.second.c_str()), 0,
        ());
    std::string const fakeNodes =
        "sponsored=partner1\n"
        "lat=-46.43525\n"
        "lon=168.35674\n"
        "banner_url=https://localads.maps.me/redirects/test\n"
        "name=Test name1\n"
        "\n"
        "sponsored=partner1\n"
        "lat=-46.43512\n"
        "lon=168.35359\n"
        "banner_url=https://localads.maps.me/redirects/test\n"
        "name=Test name2\n";
    WriteToFile(m_mixedNodesFilenames.first, fakeNodes);

    std::string const mixesTags =
        "way,548504067,sponsored=partner1,banner_url=https://localads.maps.me/redirects/"
        "test,name=Test name3\n"
        "way,548504066,sponsored=partner1,banner_url=https://localads.maps.me/redirects/"
        "test,name=Test name4\n";
    WriteToFile(m_mixedTagsFilenames.first, mixesTags);
  }

  void WriteToFile(std::string const & filename, std::string const & data)
  {
    std::ofstream stream;
    stream.exceptions(std::fstream::failbit | std::fstream::badbit);
    stream.open(filename);
    stream << data;
  }

  size_t m_threadCount;
  std::string m_testPath;
  // m_mixedNodesFilenames and m_mixedTagsFilenames contain old and new filenames.
  // This is necessary to replace a file and then restore an old one back.
  std::pair<std::string, std::string> m_mixedNodesFilenames;
  std::pair<std::string, std::string> m_mixedTagsFilenames;
  feature::GenerateInfo m_genInfo;
};

UNIT_CLASS_TEST(FeatureIntegrationTests, BuildCoasts)
{
  FeatureIntegrationTests::BuildCoasts();
}

UNIT_CLASS_TEST(FeatureIntegrationTests, BuildWorldMultithread)
{
  FeatureIntegrationTests::BuildWorld();
}

UNIT_CLASS_TEST(FeatureIntegrationTests, BuildCountriesMultithread)
{
  FeatureIntegrationTests::BuildCountries();
}

UNIT_CLASS_TEST(FeatureIntegrationTests, BuildCountriesWithComplex)
{
  FeatureIntegrationTests::BuildCountriesWithComplex();
}

UNIT_CLASS_TEST(FeatureIntegrationTests, CheckMixedTagsAndNodes)
{
  FeatureIntegrationTests::CheckMixedTagsAndNodes();
}

UNIT_CLASS_TEST(FeatureIntegrationTests, CheckGeneratedDataMultithread)
{
  FeatureIntegrationTests::CheckGeneratedData();
}

UNIT_CLASS_TEST(FeatureIntegrationTests, BuildWorldOneThread)
{
  FeatureIntegrationTests::BuildWorldOneThread();
}

UNIT_CLASS_TEST(FeatureIntegrationTests, BuildCountriesOneThread)
{
  FeatureIntegrationTests::BuildCountriesOneThread();
}

UNIT_CLASS_TEST(FeatureIntegrationTests, CheckGeneratedDataOneThread)
{
  FeatureIntegrationTests::CheckGeneratedDataOneThread();
}
