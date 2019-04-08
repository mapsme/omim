#include "generator/geo_objects/key_value_storage.hpp"

#include "base/logging.hpp"

#include <experimental/string_view>

namespace generator
{
namespace geo_objects
{
KeyValueStorage::KeyValueStorage(std::istream & stream, std::function<bool(KeyValue const &)> pred)
{
  std::string line;
  while (std::getline(stream, line))
  {
    KeyValue kv;
    if (!ParseKeyValueLine(line, kv) || !pred(kv))
      continue;

    m_values.insert(kv);
  }
}

// static
bool KeyValueStorage::ParseKeyValueLine(std::string const & line, KeyValue & res)
{
  auto const pos = line.find(" ");
  if (pos == std::string::npos)
  {
    LOG(LWARNING, ("Cannot find separator."));
    return false;
  }

  int64_t id;
  if (!strings::to_int64(line.substr(0, pos), id))
  {
    LOG(LWARNING, ("Cannot parse id."));
    return false;
  }

  base::Json json;
  try
  {
    json = base::Json(line.c_str() + pos + 1);
    if (!json.get())
      return false;
  }
  catch (base::Json::Exception const &)
  {
    LOG(LWARNING, ("Cannot create base::Json."));
    return false;
  }

  res = std::make_pair(static_cast<uint64_t>(id), json);
  return true;
}

boost::optional<base::Json> KeyValueStorage::Find(uint64_t key) const
{
  auto const it = m_values.find(key);
  if (it == std::end(m_values))
    return {};

  return it->second;
}
  
size_t KeyValueStorage::Size() const
{
  return m_values.size();
}
}  // namespace geo_objects
}  // namespace generator
