#include "storage/country.hpp"

#include "platform/mwm_version.hpp"
#include "platform/platform.hpp"

#include "base/logging.hpp"

#include "3party/jansson/myjansson.hpp"

using platform::CountryFile;

namespace storage
{
void Country::AddFile(CountryFile const & file) { m_files.push_back(file); }

////////////////////////////////////////////////////////////////////////

template <class ToDo>
void LoadGroupImpl(int depth, json_t * group, ToDo & toDo, int64_t version)
{
  if (version::IsSingleMwm(version))
  {
    for (size_t i = 0; i < json_array_size(group); ++i)
    {
      json_t * j = json_array_get(group, i);

      char const * id = json_string_value(json_object_get(j, "id"));
      if (!id)
        MYTHROW(my::Json::Exception, ("Id is missing"));

      size_t const mwmSize = static_cast<size_t>(json_integer_value(json_object_get(j, "s")));
      // @TODO(bykoianko) After we stop supporting two component mwms (with routing files)
      // rewrite toDo function to use id and mwmSize only once.
      // We expect that mwm and routing files should be less than 2GB.
      toDo(id, id,  mwmSize, 0 /* routingSize */, depth);

      json_t * children = json_object_get(j, "g");
      if (children)
        LoadGroupImpl(depth + 1, children, toDo, version);
    }
    return;
  }

  // @TODO(bykoianko) After we stop supporting two component mwms (with routing files)
  // remove code below.
  for (size_t i = 0; i < json_array_size(group); ++i)
  {
    json_t * j = json_array_get(group, i);

    // name is mandatory
    char const * name = json_string_value(json_object_get(j, "n"));
    if (!name)
      MYTHROW(my::Json::Exception, ("Country name is missing"));

    char const * file = json_string_value(json_object_get(j, "f"));
    // if file is empty, it's the same as the name
    if (!file)
      file = name;

    // We expect that mwm and routing files should be less than 2GB.
    toDo(name, file,
         static_cast<uint32_t>(json_integer_value(json_object_get(j, "s"))),
         static_cast<uint32_t>(json_integer_value(json_object_get(j, "rs"))), depth);

    json_t * children = json_object_get(j, "g");
    if (children)
      LoadGroupImpl(depth + 1, children, toDo, version);
  }
}

template <class ToDo>
bool LoadCountriesImpl(string const & jsonBuffer, ToDo & toDo, int64_t version)
{
  try
  {
    my::Json root(jsonBuffer.c_str());
    json_t * children = json_object_get(root.get(), "g");
    if (!children)
      MYTHROW(my::Json::Exception, ("Root country doesn't have any groups"));
    LoadGroupImpl(0, children, toDo, version);
    return true;
  }
  catch (my::Json::Exception const & e)
  {
    LOG(LERROR, (e.Msg()));
    return false;
  }
}

namespace
{
class DoStoreCountries
{
  CountriesContainerT & m_cont;

public:
  DoStoreCountries(CountriesContainerT & cont) : m_cont(cont) {}

  void operator()(string const & name, string const & file, uint32_t mapSize,
                  uint32_t routingSize, int depth)
  {
    Country country(name);
    if (mapSize)
    {
      CountryFile countryFile(file);
      countryFile.SetRemoteSizes(mapSize, routingSize);
      country.AddFile(countryFile);
    }
    m_cont.AddAtDepth(depth, country);
  }
};

class DoStoreFile2Info
{
  map<string, CountryInfo> & m_file2info;
  int64_t const m_version;

public:
  DoStoreFile2Info(map<string, CountryInfo> & file2info, int64_t version)
    : m_file2info(file2info), m_version(version) {}

  void operator()(string name, string file, uint32_t mapSize, uint32_t, int)
  {
    if (version::IsSingleMwm(m_version))
    {
      ASSERT_EQUAL(name, file, ());
      CountryInfo info(name);
      m_file2info[name] = move(info);
      return;
    }

    if (mapSize)
    {
      CountryInfo info;

      // if 'file' is empty - it's equal to 'name'
      if (!file.empty())
      {
        // make compound name: country_region
        size_t const i = file.find_first_of('_');
        if (i != string::npos)
          name = file.substr(0, i) + '_' + name;

        // fill 'name' only when it differs with 'file'
        if (name != file)
          info.m_name.swap(name);
      }
      else
        file.swap(name);

      m_file2info[file] = info;
    }
  }
};
}  // namespace

int64_t LoadCountries(string const & jsonBuffer, CountriesContainerT & countries)
{
  countries.Clear();

  int64_t version = -1;
  try
  {
    my::Json root(jsonBuffer.c_str());
    json_t * const rootPtr = root.get();
    version = json_integer_value(json_object_get(rootPtr, "v"));

    // Extracting root id.
    char const * const idKey = version::IsSingleMwm(version) ? "id" : "n";
    char const * id = json_string_value(json_object_get(rootPtr, idKey));
    if (!id)
      MYTHROW(my::Json::Exception, ("Id is missing"));
    Country rootCountry(id);
    // @TODO(bykoianko) Add CourtyFile to rootCountry with correct size.
    countries.Value() = rootCountry;

    DoStoreCountries doStore(countries);
    if (!LoadCountriesImpl(jsonBuffer, doStore, version))
      return -1;
  }
  catch (my::Json::Exception const & e)
  {
    LOG(LERROR, (e.Msg()));
  }
  return version;
}

void LoadCountryFile2CountryInfo(string const & jsonBuffer, map<string, CountryInfo> & id2info,
                                 bool & isSingleMwm)
{
  ASSERT(id2info.empty(), ());

  int64_t version = -1;
  try
  {
    my::Json root(jsonBuffer.c_str());
    version = json_integer_value(json_object_get(root.get(), "v"));
    isSingleMwm = version::IsSingleMwm(version);
    DoStoreFile2Info doStore(id2info, version);
    LoadCountriesImpl(jsonBuffer, doStore, version);
  }
  catch (my::Json::Exception const & e)
  {
    LOG(LERROR, (e.Msg()));
  }
}

// @TODO(@syershov) This method should be removed while removing all countries.txt generation funtionality.
template <class T>
void SaveImpl(T const & v, json_t * jParent)
{
  size_t const siblingsCount = v.SiblingsCount();
  CHECK_GREATER(siblingsCount, 0, ());

  my::JsonHandle jArray;
  jArray.AttachNew(json_array());
  for (size_t i = 0; i < siblingsCount; ++i)
  {
    my::JsonHandle jCountry;
    jCountry.AttachNew(json_object());

    string const strName = v[i].Value().Name();
    CHECK(!strName.empty(), ("Empty country name?"));
    json_object_set_new(jCountry.get(), "n", json_string(strName.c_str()));

    size_t countriesCount = v[i].Value().GetFilesCount();
    ASSERT_LESS_OR_EQUAL(countriesCount, 1, ());
    if (countriesCount > 0)
    {
      CountryFile const & file = v[i].Value().GetFile();
      string const & strFile = file.GetName();
      if (strFile != strName)
        json_object_set_new(jCountry.get(), "f", json_string(strFile.c_str()));
      json_object_set_new(jCountry.get(), "s", json_integer(file.GetRemoteSize(MapOptions::Map)));
      json_object_set_new(jCountry.get(), "rs",
                          json_integer(file.GetRemoteSize(MapOptions::CarRouting)));
    }

    if (v[i].SiblingsCount())
      SaveImpl(v[i], jCountry.get());

    json_array_append(jArray.get(), jCountry.get());
  }

  json_object_set(jParent, "g", jArray.get());
}

// @TODO(@syershov) This method should be removed while removing all countries.txt generation funtionality.
bool SaveCountries(int64_t version, CountriesContainerT const & countries, string & jsonBuffer)
{
  my::JsonHandle root;
  root.AttachNew(json_object());

  json_object_set_new(root.get(), "v", json_integer(version));
  json_object_set_new(root.get(), "n", json_string("World"));
  SaveImpl(countries, root.get());

  char * res = json_dumps(root.get(), JSON_PRESERVE_ORDER | JSON_COMPACT | JSON_INDENT(1));
  jsonBuffer = res;
  free(res);
  return true;
}
}  // namespace storage
