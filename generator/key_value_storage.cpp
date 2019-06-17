#include "generator/key_value_storage.hpp"

#include "coding/reader.hpp"

#include "base/exception.hpp"
#include "base/logging.hpp"

namespace generator
{
KeyValueStorage::KeyValueStorage(std::string const & path,
                                 std::function<bool(KeyValue const &)> const & pred)
  : m_storage{path, std::ios_base::in | std::ios_base::out | std::ios_base::app}
{
  if (!m_storage)
    MYTHROW(Reader::OpenException, ("Failed to open file", path));

  std::string line;
  std::streamoff lineNumber = 0;
  while (std::getline(m_storage, line))
  {
    ++lineNumber;

    KeyValue kv;
    if (!ParseKeyValueLine(line, kv, lineNumber) || !pred(kv))
      continue;

    m_values.insert(kv);
  }

  m_storage.clear();
}

// static
bool KeyValueStorage::ParseKeyValueLine(std::string const & line, KeyValue & res, std::streamoff lineNumber)
{
  auto const pos = line.find(" ");
  if (pos == std::string::npos)
  {
    LOG(LWARNING, ("Cannot find separator in line", lineNumber));
    return false;
  }

  int64_t id;
  if (!strings::to_int64(line.substr(0, pos), id))
  {
    LOG(LWARNING, ("Cannot parse id", line.substr(0, pos) , "in line", lineNumber));
    return false;
  }

  auto jsonString = line.c_str() + pos + 1;
  json_error_t jsonError;
  base::JSONPtr json{json_loads(jsonString, 0, &jsonError)};
  if (!json)
  {
    LOG(LWARNING, ("Cannot create base::Json in line", lineNumber, ":", jsonError.text));
    return false;
  }

  res = std::make_pair(static_cast<uint64_t>(id), std::make_shared<JsonValue>(std::move(json)));
  return true;
}

void KeyValueStorage::Insert(uint64_t key, JsonString && valueJson, base::JSONPtr && value)
{
  auto const emplace = m_values.emplace(key, std::make_shared<JsonValue>(std::move(value)));
  if (!emplace.second) // it is ok for OSM relation with several outer borders
    return;

  m_storage << static_cast<int64_t>(key) << " " << valueJson.get() << "\n";
}

std::shared_ptr<JsonValue> KeyValueStorage::Find(uint64_t key) const
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
}  // namespace generator
