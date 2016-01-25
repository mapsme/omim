#include "testing/testing.hpp"
#include "testing/testregister.hpp"

#include "base/logging.hpp"
#include "base/string_utils.hpp"
#include "base/timer.hpp"

#include "std/cstring.hpp"
#include "std/iomanip.hpp"
#include "std/iostream.hpp"
#include "std/regex.hpp"
#include "std/string.hpp"
#include "std/target_os.hpp"
#include "std/vector.hpp"

#ifndef OMIM_UNIT_TEST_DISABLE_PLATFORM_INIT
# include "platform/platform.hpp"
#endif

#ifdef OMIM_UNIT_TEST_WITH_QT_EVENT_LOOP
  #include <QtCore/Qt>
  #ifdef OMIM_OS_MAC // on Mac OS X native run loop works only for QApplication :(
    #if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
      #include <QtGui/QApplication>
    #else
      #include <QtWidgets/QApplication>
    #endif
    #define QAPP QApplication
  #else
    #include <QtCore/QCoreApplication>
    #define QAPP QCoreApplication
  #endif
#endif

namespace
{
bool g_bLastTestOK = true;
CommandLineOptions g_testingOptions;

int const kOptionFieldWidth = 32;
char const kFilterOption[] = "--filter=";
char const kSuppressOption[] = "--suppress=";
char const kHelpOption[] = "--help";
char const kDataPathOptions[] = "--data_path=";
char const kResourcePathOptions[] = "--user_resource_path=";
char const kListAllTestsOption[] = "--list_tests";

enum Status
{
  STATUS_SUCCESS = 0,
  STATUS_FAILED = 1,
  STATUS_BROKEN_FRAMEWORK = 5,
};

void DisplayOption(ostream & os, char const * option, char const * description)
{
  os << "  " << setw(kOptionFieldWidth) << left << option << " " << description << endl;
}

void DisplayOption(ostream & os, char const * option, char const * value, char const * description)
{
  os << "  " << setw(kOptionFieldWidth) << left << (string(option) + string(value)) << " "
     << description << endl;
}

void Usage(char const * name)
{
  cerr << "USAGE: " << name << " [options]" << endl;
  cerr << endl;
  cerr << "OPTIONS:" << endl;
  DisplayOption(cerr, kFilterOption, "<ECMA Regexp>",
                "Run tests with names corresponding to regexp.");
  DisplayOption(cerr, kSuppressOption, "<ECMA Regexp>",
                "Do not run tests with names corresponding to regexp.");
  DisplayOption(cerr, kDataPathOptions, "<Path>", "Path to data files.");
  DisplayOption(cerr, kResourcePathOptions, "<Path>", "Path to resources, styles and classificators.");
  DisplayOption(cerr, kListAllTestsOption, "List all the tests in the test suite and exit.");
  DisplayOption(cerr, kHelpOption, "Print this help message and exit.");
}

void ParseOptions(int argc, char * argv[], CommandLineOptions & options)
{
  for (int i = 1; i < argc; ++i)
  {
    char const * const arg = argv[i];
    if (strings::StartsWith(arg, kFilterOption))
      options.m_filterRegExp = arg + sizeof(kFilterOption) - 1;
    if (strings::StartsWith(arg, kSuppressOption))
      options.m_suppressRegExp = arg + sizeof(kSuppressOption) - 1;
    if (strings::StartsWith(arg, kDataPathOptions))
      options.m_dataPath = arg + sizeof(kDataPathOptions) - 1;
    if (strings::StartsWith(arg, kResourcePathOptions))
      options.m_resourcePath = arg + sizeof(kResourcePathOptions) - 1;
    if (strcmp(arg, kHelpOption) == 0)
      options.m_help = true;
    if (strcmp(arg, kListAllTestsOption) == 0)
      options.m_listTests = true;
  }
}
}  // namespace

CommandLineOptions const & GetTestingOptions()
{
  return g_testingOptions;
}

int main(int argc, char * argv[])
{
#ifdef OMIM_UNIT_TEST_WITH_QT_EVENT_LOOP
  QAPP theApp(argc, argv);
  UNUSED_VALUE(theApp);
#else
  UNUSED_VALUE(argc);
  UNUSED_VALUE(argv);
#endif

  my::ScopedLogLevelChanger const infoLogLevel(LINFO);
#if defined(OMIM_OS_MAC) || defined(OMIM_OS_LINUX)
  my::SetLogMessageFn(my::LogMessageTests);
#endif

  vector<string> testNames;
  vector<bool> testResults;
  int numFailedTests = 0;

  ParseOptions(argc, argv, g_testingOptions);
  if (g_testingOptions.m_help)
  {
    Usage(argv[0]);
    return STATUS_SUCCESS;
  }

  regex filterRegExp;
  if (g_testingOptions.m_filterRegExp)
    filterRegExp.assign(g_testingOptions.m_filterRegExp);

  regex suppressRegExp;
  if (g_testingOptions.m_suppressRegExp)
    suppressRegExp.assign(g_testingOptions.m_suppressRegExp);

#ifndef OMIM_UNIT_TEST_DISABLE_PLATFORM_INIT
  // Setting stored paths from testingmain.cpp
  Platform & pl = GetPlatform();
  CommandLineOptions const & options = GetTestingOptions();
  if (options.m_dataPath)
    pl.SetWritableDirForTests(options.m_dataPath);
  if (options.m_resourcePath)
    pl.SetResourceDir(options.m_resourcePath);
#endif

  for (TestRegister * pTest = TestRegister::FirstRegister(); pTest; pTest = pTest->m_pNext)
  {
    string fileName(pTest->m_FileName);
    string testName(pTest->m_TestName);

    // Retrieve fine file name
    auto const lastSlash = fileName.find_last_of("\\/");
    if (lastSlash != string::npos)
      fileName.erase(0, lastSlash + 1);

    testNames.push_back(fileName + "::" + testName);
    testResults.push_back(true);
  }

  if (GetTestingOptions().m_listTests)
  {
    for (auto const & name : testNames)
      cout << name << endl;
    return 0;
  }

  int iTest = 0;
  for (TestRegister * pTest = TestRegister::FirstRegister(); pTest; ++iTest, pTest = pTest->m_pNext)
  {
    auto const & testName = testNames[iTest];
    if (g_testingOptions.m_filterRegExp &&
        !regex_match(testName.begin(), testName.end(), filterRegExp))
    {
      continue;
    }
    if (g_testingOptions.m_suppressRegExp &&
        regex_match(testName.begin(), testName.end(), suppressRegExp))
    {
      continue;
    }

    LOG(LINFO, ("Running", testName));
    if (!g_bLastTestOK)
    {
      // Somewhere else global variables have been reset.
      LOG(LERROR, ("\n\nSOMETHING IS REALLY WRONG IN THE UNIT TEST FRAMEWORK!!!"));
      return STATUS_BROKEN_FRAMEWORK;
    }

    my::HighResTimer timer(true);

    try
    {
      // Run the test.
      pTest->m_Fn();

      if (g_bLastTestOK)
      {
        LOG(LINFO, ("OK"));
      }
      else
      {
        // You can set Break here if test failed,
        // but it is already set in OnTestFail - to fail immediately.
        testResults[iTest] = false;
        ++numFailedTests;
      }

    }
    catch (TestFailureException const & )
    {
      testResults[iTest] = false;
      ++numFailedTests;
    }
    catch (std::exception const & ex)
    {
      LOG(LERROR, ("FAILED", "<<<Exception thrown [", ex.what(), "].>>>"));
      testResults[iTest] = false;
      ++numFailedTests;
    }
    catch (...)
    {
      LOG(LERROR, ("FAILED<<<Unknown exception thrown.>>>"));
      testResults[iTest] = false;
      ++numFailedTests;
    }
    g_bLastTestOK = true;

    uint64_t const elapsed = timer.ElapsedNano();
    LOG(LINFO, ("Test took", elapsed / 1000000, "ms\n"));
  }

  if (numFailedTests != 0)
  {
    LOG(LINFO, (numFailedTests, " tests failed:"));
    for (size_t i = 0; i < testNames.size(); ++i)
    {
      if (!testResults[i])
        LOG(LINFO, (testNames[i]));
    }
    LOG(LINFO, ("Some tests FAILED."));
    return STATUS_FAILED;
  }

  LOG(LINFO, ("All tests passed."));
  return STATUS_SUCCESS;
}
