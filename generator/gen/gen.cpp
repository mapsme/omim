#include "common.hpp"

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <utility>

#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <boost/process.hpp>
#include <boost/program_options.hpp>

namespace pc = boost::process;
namespace po = boost::program_options;
namespace fs = boost::filesystem;

namespace
{
static const std::map<std::string, std::string> kCommands =
{
  // Binary name without prefix, description.
  {"preprocess", "Create nodes/ways/relations data."},
};

bool CommandIsValid(std::string const & command)
{
  return kCommands.count(command) != 0;
}

std::pair<std::string, int> FindCommandInArgs(int argc, char ** argv)
{
  for (int i = 1; i < argc; ++i)
  {
    if (argv[i][0] != '-')
      return std::make_pair(argv[i], i + 1);
  }

  return {};
}

void Help(po::options_description const & desc)
{
  std::cout << desc << "\n";
  std::cout << "These are common gen commands used in various situations:" << "\n";
  for (auto const & c : kCommands)
    std::cout << std::setw(16) << std::left << c.first << c.second << "\n";

  std::cout << std::endl;
}

std::string GetBinaryPath(std::string const & command, std::string const & execPath)
{
  std::string const kPrefix = "gen-";
  return fs::absolute(execPath)
      .append(kPrefix + command)
      .string();
}

int RunCommand(std::string const & path, std::vector<std::string> && args)
{
  auto env = boost::this_process::environment();
  auto child = pc::child(path, pc::args(std::move(args)), env);
  child.wait();
  return child.exit_code();
}

std::vector<std::string> GetCommandParams(int argc, char ** argv, int pos)
{
  std::vector<std::string> result;
  for (int i = pos; i < argc; ++i)
    result.push_back(argv[i]);

  return result;
}

boost::optional<std::string>
ValidateArgs(po::parsed_options const & parsed, po::variables_map const & desc)
{
  for (auto const & o : parsed.options)
  {
      if (desc.find(o.string_key) == std::end(desc))
      {
        if (!o.string_key.empty())
          return o.string_key;
        else
          return {};
      }
  }

  return {};
}
}  // namespace

int main(int argc, char ** argv)
{
  try
  {
    po::options_description desc("usage: gen [--help] [--exec-path=<path>] <command> [<args>]");
    desc.add_options()
        ("help,h", "Prints the synopsis and a list of the most commonly used commands.")
        ("exec-path,e", po::value<std::string>()->value_name("PATH"),
         "Path to wherever your core gen programs are installed. This can also be controlled by setting "
         "the GEN_EXEC_PATH environment variable.");

    po::variables_map vm;
    po::parsed_options parsed = po::command_line_parser(argc, argv)
                                .options(desc)
                                .allow_unregistered()
                                .run();
    po::store(parsed, vm);
    po::notify(vm);

    auto const val = ValidateArgs(parsed, vm);
    if (val)
    {
      std::cout << "Unknown option: " << *val << ".\n";
      Help(desc);
      return EXIT_FAILURE;
    }

    if (vm.count("help"))
    {
      Help(desc);
      return EXIT_SUCCESS;
    }

    std::string optionExecPath;
    char const * genExecPathVar = std::getenv("GEN_EXEC_PATH");
    if (genExecPathVar)
      optionExecPath = genExecPathVar;
    if (vm.count("exec-path"))
      optionExecPath = vm.at("exec-path").as<std::string>();

    auto const program = FindCommandInArgs(argc, argv);
    if (!program.second)
    {
      std::cout << "You did not specify a command.\n";
      Help(desc);
      return EXIT_FAILURE;
    }

    if (!CommandIsValid(program.first))
    {
      std::cout << "Command " << program.first << " is not valid.\n";
      Help(desc);
      return EXIT_FAILURE;
    }

    auto const fullBinaryPath = GetBinaryPath(program.first, optionExecPath);
    if (!fs::exists(fullBinaryPath))
    {
      std::cout << fullBinaryPath << " was not found.\n";
      std::cout << "Please set path variable GEN_EXEC_PATH or option --exec-path=<path> "
                   "to binary files.\n";
      Help(desc);
      return EXIT_FAILURE;
    }

    auto params = GetCommandParams(argc, argv, program.second);
    return RunCommand(fullBinaryPath, std::move(params));
  }
  catch (po::error const & err)
  {
    std::cout << "Error: " << err.what() << std::endl;
    return EXIT_FAILURE;
  }
}
