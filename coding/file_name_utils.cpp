#include "file_name_utils.hpp"

#include "std/target_os.hpp"


namespace my
{
void GetNameWithoutExt(std::string & name)
{
  std::string::size_type const i = name.rfind('.');
  if (i != std::string::npos)
    name.erase(i);
}

std::string FilenameWithoutExt(std::string name)
{
  GetNameWithoutExt(name);
  return name;
}

std::string GetFileExtension(std::string const & name)
{
  size_t const pos = name.find_last_of("./\\");
  return ((pos != std::string::npos && name[pos] == '.') ? name.substr(pos) : std::string());
}

void GetNameFromFullPath(std::string & name)
{
  std::string::size_type const i = name.find_last_of("/\\");
  if (i != std::string::npos)
    name = name.substr(i+1);
}

std::string GetDirectory(std::string const & name)
{
  std::string const sep = GetNativeSeparator();
  size_t const sepSize = sep.size();

  std::string::size_type i = name.rfind(sep);
  if (i == std::string::npos)
    return ".";
  while (i > sepSize && (name.substr(i - sepSize, sepSize) == sep))
    i -= sepSize;
  return i == 0 ? sep : name.substr(0, i);
}

std::string GetNativeSeparator()
{
#ifdef OMIM_OS_WINDOWS
    return "\\";
#else
    return "/";
#endif
}

std::string JoinFoldersToPath(const std::string & folder, const std::string & file)
{
  return my::AddSlashIfNeeded(folder) + file;
}

std::string JoinFoldersToPath(std::initializer_list<std::string> const & folders, const std::string & file)
{
  std::string result;
  for (std::string const & s : folders)
    result += AddSlashIfNeeded(s);

  result += file;
  return result;
}

std::string AddSlashIfNeeded(std::string const & path)
{
  std::string const sep = GetNativeSeparator();
  std::string::size_type const pos = path.rfind(sep);
  if ((pos != std::string::npos) && (pos + sep.size() == path.size()))
    return path;
  return path + sep;
}

}  // namespace my
