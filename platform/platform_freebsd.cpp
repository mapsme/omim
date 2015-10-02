#include "platform/platform.hpp"

#include "base/logging.hpp"
#include "coding/file_reader.hpp"

#include <stdlib.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysctl.h>


/// @return directory where binary resides, including slash at the end
static bool GetBinaryFolder(string & outPath)
{
  int mib[4];
  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC;
  mib[2] = KERN_PROC_PATHNAME;
  mib[3] = -1;
  char path[4096];
  size_t cb = ARRAY_SIZE(path);
  if (::sysctl(mib, 4, path, &cb, NULL, 0) == 0) {
    outPath = path;
    outPath.erase(outPath.find_last_of('/') + 1);
    return true;
  }
  return false;
}

Platform::Platform()
{
  // init directories
  string path;
  CHECK(GetBinaryFolder(path), ("Can't retrieve path to executable"));

  char const * homePath = ::getenv("HOME");
  string const home(homePath ? homePath : "");

  m_settingsDir = home + "/.config/MapsWithMe";

  if (!IsFileExistsByFullPath(m_settingsDir + SETTINGS_FILE_NAME))
  {
    mkdir((home + "/.config/").c_str(), 0755);
    mkdir(m_settingsDir.c_str(), 0755);
  }

  m_writableDir = home + "/.local/share/MapsWithMe";
  mkdir((home + "/.local/").c_str(), 0755);
  mkdir((home + "/.local/share/").c_str(), 0755);
  mkdir(m_writableDir.c_str(), 0755);

  char const * resDir = ::getenv("MWM_RESOURCES_DIR");
  if (resDir)
    m_resourcesDir = resDir;
  else
  {
    // developer builds with symlink
    if (IsFileExistsByFullPath(path + "../../data/eula.html"))
    {
      m_resourcesDir = path + "../../data";
      m_writableDir = m_resourcesDir;
    }
    // developer builds without symlink
    else if (IsFileExistsByFullPath(path + "../../../omim/data/eula.html"))
    {
      m_resourcesDir = path + "../../../omim/data";
      m_writableDir = m_resourcesDir;
    }
    // installed version - /opt/MapsWithMe and unpacked packages
    else if (IsFileExistsByFullPath(path + "../share/eula.html"))
      m_resourcesDir = path + "../share";
    // installed version
    else if (IsFileExistsByFullPath(path + "../share/MapsWithMe/eula.html"))
      m_resourcesDir = path + "../share/MapsWithMe";
    // all-nearby installs
    else if (IsFileExistsByFullPath(path + "/eula.html"))
      m_resourcesDir = path;
  }
  m_resourcesDir += '/';
  m_settingsDir += '/';
  m_writableDir += '/';

  char const * tmpDir = ::getenv("TMPDIR");
  if (tmpDir)
    m_tmpDir = tmpDir;
  else
    m_tmpDir = "/tmp";
  m_tmpDir += '/';

  LOG(LDEBUG, ("Resources directory:", m_resourcesDir));
  LOG(LDEBUG, ("Writable directory:", m_writableDir));
  LOG(LDEBUG, ("Tmp directory:", m_tmpDir));
  LOG(LDEBUG, ("Settings directory:", m_settingsDir));
  LOG(LDEBUG, ("Client ID:", UniqueClientId()));
}

string Platform::UniqueClientId() const
{
  bool filterdashes = false;
  string machineFile = "/var/db/dbus/machine-id";
  if (IsFileExistsByFullPath("/etc/hostid")) {
    machineFile = "/etc/hostid";
    filterdashes = true;
  }

  if (IsFileExistsByFullPath(machineFile))
  {
    string content;
    FileReader(machineFile).ReadAsString(content);

    if (filterdashes) {
      string tmp;
	  std::remove_copy(content.begin(), content.end(), std::back_inserter(tmp), '-');
      content = tmp;
    }

    return content.substr(0, 32);
  }
  else
    return "n0dbus0n0hostid00000000000000000";

}

void Platform::RunOnGuiThread(TFunctor const & fn)
{
  /// @todo
  fn();
}

void Platform::RunAsync(TFunctor const & fn, Priority p)
{
  /// @todo
  fn();
}

Platform::EConnectionType Platform::ConnectionStatus()
{
  // @TODO Add implementation
  return EConnectionType::CONNECTION_NONE;
}
