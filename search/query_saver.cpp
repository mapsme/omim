#include "query_saver.hpp"

#include "platform/settings.hpp"

#include "coding/base64.hpp"
#include "coding/reader.hpp"
#include "coding/writer.hpp"
#include "coding/write_to_sink.hpp"

#include "base/logging.hpp"
#include "base/stl_add.hpp"
#include "base/string_utils.hpp"

#include <limits>

namespace
{
char constexpr kSettingsKey[] = "UserQueries";
using TLength = uint16_t;
TLength constexpr kMaxSuggestionsCount = 10;

// Reader from memory that throws exceptions.
class SecureMemReader : public Reader
{
  void CheckPosAndSize(uint64_t pos, uint64_t size) const
  {
    if (pos + size > m_size || size > std::numeric_limits<size_t>::max())
      MYTHROW(SizeException, (pos, size, m_size) );
  }

public:
  // Construct from block of memory.
  SecureMemReader(void const * pData, size_t size)
    : m_pData(static_cast<char const *>(pData)), m_size(size)
  {
  }

  inline uint64_t Size() const override
  {
    return m_size;
  }

  inline void Read(uint64_t pos, void * p, size_t size) const override
  {
    CheckPosAndSize(pos, size);
    memcpy(p, m_pData + pos, size);
  }

  inline SecureMemReader SubReader(uint64_t pos, uint64_t size) const
  {
    CheckPosAndSize(pos, size);
    return SecureMemReader(m_pData + pos, static_cast<size_t>(size));
  }

  inline std::unique_ptr<Reader> CreateSubReader(uint64_t pos, uint64_t size) const override
  {
    CheckPosAndSize(pos, size);
    return my::make_unique<SecureMemReader>(m_pData + pos, static_cast<size_t>(size));
  }

private:
  char const * m_pData;
  size_t m_size;
};

}  // namespace

namespace search
{
QuerySaver::QuerySaver()
{
  Load();
}

void QuerySaver::Add(TSearchRequest const & query)
{
  // This change was made just before release, so we don't use untested search normalization methods.
  //TODO (ldragunov) Rewrite to normalized requests.
  TSearchRequest trimmedQuery(query);
  strings::Trim(trimmedQuery.first);
  strings::Trim(trimmedQuery.second);
  auto trimmedComparator = [&trimmedQuery](TSearchRequest request)
    {
      strings::Trim(request.first);
      strings::Trim(request.second);
      return trimmedQuery == request;
    };
  // Remove items if needed.
  auto const it = find_if(m_topQueries.begin(), m_topQueries.end(), trimmedComparator);
  if (it != m_topQueries.end())
    m_topQueries.erase(it);
  else if (m_topQueries.size() >= kMaxSuggestionsCount)
    m_topQueries.pop_back();

  // Add new query and save it to drive.
  m_topQueries.push_front(query);
  Save();
}

void QuerySaver::Clear()
{
  m_topQueries.clear();
  settings::Delete(kSettingsKey);
}

void QuerySaver::Serialize(std::string & data) const
{
  std::vector<uint8_t> rawData;
  MemWriter<std::vector<uint8_t>> writer(rawData);
  TLength size = m_topQueries.size();
  WriteToSink(writer, size);
  for (auto const & query : m_topQueries)
  {
    size = query.first.size();
    WriteToSink(writer, size);
    writer.Write(query.first.c_str(), size);
    size = query.second.size();
    WriteToSink(writer, size);
    writer.Write(query.second.c_str(), size);
  }
  data = base64::Encode(std::string(rawData.begin(), rawData.end()));
}

void QuerySaver::Deserialize(std::string const & data)
{
  std::string decodedData = base64::Decode(data);
  SecureMemReader rawReader(decodedData.c_str(), decodedData.size());
  ReaderSource<SecureMemReader> reader(rawReader);

  TLength queriesCount = ReadPrimitiveFromSource<TLength>(reader);
  queriesCount = std::min(queriesCount, kMaxSuggestionsCount);

  for (TLength i = 0; i < queriesCount; ++i)
  {
    TLength localeLength = ReadPrimitiveFromSource<TLength>(reader);
    std::vector<char> locale(localeLength);
    reader.Read(&locale[0], localeLength);
    TLength stringLength = ReadPrimitiveFromSource<TLength>(reader);
    std::vector<char> str(stringLength);
    reader.Read(&str[0], stringLength);
    m_topQueries.emplace_back(std::make_pair(std::string(&locale[0], localeLength),
                                        std::string(&str[0], stringLength)));
  }
}

void QuerySaver::Save()
{
  std::string data;
  Serialize(data);
  settings::Set(kSettingsKey, data);
}

void QuerySaver::Load()
{
  std::string hexData;
  settings::Get(kSettingsKey, hexData);
  if (hexData.empty())
    return;
  try
  {
    Deserialize(hexData);
  }
  catch (Reader::SizeException const & /* exception */)
  {
    Clear();
    LOG(LWARNING, ("Search history data corrupted! Creating new one."));
  }
}
}  // namesapce search
