#include "ugc/storage.hpp"
#include "ugc/serdes.hpp"
#include "ugc/serdes_json.hpp"

#include "indexer/classificator.hpp"
#include "indexer/feature_algo.hpp"
#include "indexer/feature_decl.hpp"
#include "indexer/ftraits.hpp"
#include "indexer/index.hpp"

#include "platform/platform.hpp"

#include "coding/file_name_utils.hpp"
#include "coding/file_reader.hpp"
#include "coding/file_writer.hpp"
#include "coding/internal/file_data.hpp"

#include <algorithm>
#include <utility>

#include "3party/jansson/myjansson.hpp"

namespace ugc
{
using namespace std;

namespace
{
string const kIndexFileName = "index.json";
string const kUGCUpdateFileName = "ugc.update.bin";
string const kTmpFileExtension = ".tmp";

using Sink = MemWriter<string>;

string GetUGCFilePath() { return my::JoinPath(GetPlatform().SettingsDir(), kUGCUpdateFileName); }

string GetIndexFilePath() { return my::JoinPath(GetPlatform().SettingsDir(), kIndexFileName); }

bool GetUGCFileSize(uint64_t & size)
{
  try
  {
    FileReader reader(GetUGCFilePath(), true /* with exceptions */);
    size = reader.Size();
  }
  catch (RootException const &)
  {
    return false;
  }

  return true;
}

void DeserializeUGCIndex(string const & jsonData, vector<Storage::UGCIndex> & res)
{
  if (jsonData.empty())
    return;

  DeserializerJsonV0 des(jsonData);
  des(res);
}

string SerializeUGCIndex(vector<Storage::UGCIndex> const & indexes)
{
  if (indexes.empty())
    return string();

  auto array = my::NewJSONArray();
  for (auto const & index : indexes)
  {
    string data;
    {
      Sink sink(data);
      SerializerJson<Sink> ser(sink);
      ser(index);
    }

    my::Json node(data);
    json_array_append_new(array.get(), node.get_deep_copy());
  }

  unique_ptr<char, JSONFreeDeleter> buffer(json_dumps(array.get(), JSON_COMPACT | JSON_ENSURE_ASCII));
  return string(buffer.get());
}

template <typename UGCUpdate>
Storage::SettingResult SetGenericUGCUpdate(
    vector<Storage::UGCIndex> & indexes, size_t & numberOfDeleted, FeatureID const & id,
    UGCUpdate const & ugc,
    FeatureType const & featureType,
    Version const version = Version::Latest)
{
  if (!ugc.IsValid())
    return Storage::SettingResult::InvalidUGC;

  auto const mercator = feature::GetCenter(featureType);
  feature::TypesHolder th(featureType);
  th.SortBySpec();
  auto const optMatchingType = ftraits::UGC::GetType(th);
  CHECK(optMatchingType, ());
  auto const type = th.GetBestType();
  for (auto & index : indexes)
  {
    if (type == index.m_type && mercator == index.m_mercator && !index.m_deleted)
    {
      index.m_deleted = true;
      ++numberOfDeleted;
      break;
    }
  }

  Storage::UGCIndex index;
  uint64_t offset;
  if (!GetUGCFileSize(offset))
    offset = 0;

  index.m_mercator = mercator;
  index.m_type = type;
  index.m_matchingType = *optMatchingType;
  index.m_mwmName = id.GetMwmName();
  index.m_dataVersion = id.GetMwmVersion();
  index.m_featureId = id.m_index;
  index.m_offset = offset;

  auto const ugcFilePath = GetUGCFilePath();
  try
  {
    FileWriter w(ugcFilePath, FileWriter::Op::OP_APPEND);
    Serialize(w, ugc, version);
    indexes.emplace_back(move(index));
  }
  catch (FileWriter::Exception const & exception)
  {
    LOG(LERROR, ("Exception while writing file:", ugcFilePath, "reason:", exception.what()));
    return Storage::SettingResult::WritingError;
  }

  return Storage::SettingResult::Success;
}
}  // namespace

UGCUpdate Storage::GetUGCUpdate(FeatureID const & id) const
{
  if (m_UGCIndexes.empty())
    return {};

  auto const feature = GetFeature(id);
  auto const mercator = feature::GetCenter(*feature);
  feature::TypesHolder th(*feature);
  th.SortBySpec();
  auto const type = th.GetBestType();

  auto const index = find_if(
      m_UGCIndexes.begin(), m_UGCIndexes.end(), [type, &mercator](UGCIndex const & index) -> bool {
        return type == index.m_type && mercator == index.m_mercator && !index.m_deleted;
      });

  if (index == m_UGCIndexes.end())
    return {};

  auto const offset = index->m_offset;
  auto const size = static_cast<size_t>(UGCSizeAtIndex(distance(m_UGCIndexes.begin(), index)));
  vector<uint8_t> buf;
  buf.resize(size);
  auto const ugcFilePath = GetUGCFilePath();
  try
  {
    FileReader r(ugcFilePath);
    r.Read(offset, buf.data(), size);
  }
  catch (FileReader::Exception const & exception)
  {
    LOG(LERROR, ("Exception while reading file:", ugcFilePath, "reason:", exception.what()));
    return {};
  }

  MemReader r(buf.data(), buf.size());
  NonOwningReaderSource source(r);
  UGCUpdate update;
  Deserialize(source, update);
  return update;
}

Storage::SettingResult Storage::SetUGCUpdate(FeatureID const & id, UGCUpdate const & ugc)
{
  auto const feature = GetFeature(id);
  return SetGenericUGCUpdate(m_UGCIndexes, m_numberOfDeleted, id, ugc,
                             *feature);
}

void Storage::Load()
{
  string data;
  auto const indexFilePath = GetIndexFilePath();
  try
  {
    FileReader r(indexFilePath);
    r.ReadAsString(data);
  }
  catch (FileReader::Exception const & exception)
  {
    LOG(LWARNING, ("Exception while reading file:", indexFilePath, "reason:", exception.what()));
    return;
  }

  DeserializeUGCIndex(data, m_UGCIndexes);
  for (auto const & i : m_UGCIndexes)
  {
    if (i.m_deleted)
      ++m_numberOfDeleted;
  }
}

void Storage::SaveIndex() const
{
  if (m_UGCIndexes.empty())
    return;

  auto const jsonData = SerializeUGCIndex(m_UGCIndexes);
  auto const indexFilePath = GetIndexFilePath();
  try
  {
    FileWriter w(indexFilePath);
    w.Write(jsonData.c_str(), jsonData.length());
  }
  catch (FileWriter::Exception const & exception)
  {
    LOG(LERROR, ("Exception while writing file:", indexFilePath, "reason:", exception.what()));
  }
}

void Storage::Defragmentation()
{
  auto const indexesSize = m_UGCIndexes.size();
  if (m_numberOfDeleted < indexesSize / 2)
    return;

  auto const ugcFilePath = GetUGCFilePath();
  auto const tmpUGCFilePath = ugcFilePath + kTmpFileExtension;

  try
  {
    FileReader r(ugcFilePath);
    FileWriter w(tmpUGCFilePath, FileWriter::Op::OP_APPEND);
    uint64_t actualOffset = 0;
    for (size_t i = 0; i < indexesSize; ++i)
    {
      auto & index = m_UGCIndexes[i];
      if (index.m_deleted)
        continue;

      auto const offset = index.m_offset;
      auto const size = static_cast<size_t>(UGCSizeAtIndex(i));
      vector<uint8_t> buf;
      buf.resize(size);
      r.Read(offset, buf.data(), size);
      w.Write(buf.data(), size);
      index.m_offset = actualOffset;
      actualOffset += size;
    }
  }
  catch (FileReader::Exception const & exception)
  {
    LOG(LERROR, ("Exception while reading file:", ugcFilePath, "reason:", exception.what()));
    return;
  }
  catch (FileWriter::Exception const & exception)
  {
    LOG(LERROR, ("Exception while writing file:", tmpUGCFilePath, "reason:", exception.what()));
    return;
  }

  m_UGCIndexes.erase(remove_if(m_UGCIndexes.begin(), m_UGCIndexes.end(),
                               [](UGCIndex const & i) -> bool { return i.m_deleted; }), m_UGCIndexes.end());

  CHECK(my::DeleteFileX(ugcFilePath), ());
  CHECK(my::RenameFileX(tmpUGCFilePath, ugcFilePath), ());

  m_numberOfDeleted = 0;
}

string Storage::GetUGCToSend() const
{
  if (m_UGCIndexes.empty())
    return string();

  auto array = my::NewJSONArray();
  auto const indexesSize = m_UGCIndexes.size();
  auto const ugcFilePath = GetUGCFilePath();
  FileReader r(ugcFilePath);
  vector<uint8_t> buf;
  for (size_t i = 0; i < indexesSize; ++i)
  {
    buf.clear();
    auto const & index = m_UGCIndexes[i];
    if (index.m_synchronized || index.m_deleted)
      continue;

    auto const offset = index.m_offset;
    auto const bufSize = static_cast<size_t>(UGCSizeAtIndex(i));
    buf.resize(bufSize);
    try
    {
      r.Read(offset, buf.data(), bufSize);
    }
    catch (FileReader::Exception const & exception)
    {
      LOG(LERROR, ("Exception while reading file:", ugcFilePath, "reason:", exception.what()));
      return string();
    }

    MemReader r(buf.data(), buf.size());
    NonOwningReaderSource source(r);
    UGCUpdate update;
    Deserialize(source, update);

    string data;
    {
      Sink sink(data);
      SerializerJson<Sink> ser(sink);
      ser(update);
    }

    my::Json serializedUgc(data);
    auto embeddedNode = my::NewJSONObject();
    ToJSONObject(*embeddedNode.get(), "data_version", index.m_dataVersion);
    ToJSONObject(*embeddedNode.get(), "mwm_name", index.m_mwmName);
    ToJSONObject(*embeddedNode.get(), "feature_id", index.m_featureId);
    ToJSONObject(*embeddedNode.get(), "feature_type", classif().GetReadableObjectName(index.m_matchingType));
    ToJSONObject(*serializedUgc.get(), "feature", *embeddedNode.release());
    json_array_append_new(array.get(), serializedUgc.get_deep_copy());
  }

  if (json_array_size(array.get()) == 0)
    return string();

  auto reviewsNode = my::NewJSONObject();
  ToJSONObject(*reviewsNode.get(), "reviews", *array.release());

  unique_ptr<char, JSONFreeDeleter> buffer(json_dumps(reviewsNode.get(), JSON_COMPACT | JSON_ENSURE_ASCII));
  return string(buffer.get());
}

void Storage::MarkAllAsSynchronized()
{
  if (m_UGCIndexes.empty())
    return;

  size_t numberOfUnsynchronized = 0;
  for (auto & index : m_UGCIndexes)
  {
    if (!index.m_synchronized)
    {
      index.m_synchronized = true;
      numberOfUnsynchronized++;
    }
  }

  if (numberOfUnsynchronized == 0)
    return;

  auto const indexPath = GetIndexFilePath();
  my::DeleteFileX(indexPath);
  SaveIndex();
}

uint64_t Storage::UGCSizeAtIndex(size_t const indexPosition) const
{
  CHECK(!m_UGCIndexes.empty(), ());
  auto const indexesSize = m_UGCIndexes.size();
  CHECK_LESS(indexPosition, indexesSize, ());
  auto const indexOffset = m_UGCIndexes[indexPosition].m_offset;
  uint64_t nextOffset;
  if (indexPosition == indexesSize - 1)
    CHECK(GetUGCFileSize(nextOffset), ());
  else
    nextOffset = m_UGCIndexes[indexPosition + 1].m_offset;

  CHECK_GREATER(nextOffset, indexOffset, ());
  return nextOffset - indexOffset;
}

unique_ptr<FeatureType> Storage::GetFeature(FeatureID const & id) const
{
  CHECK(id.IsValid(), ());
  Index::FeaturesLoaderGuard guard(m_index, id.m_mwmId);
  auto feature = guard.GetOriginalOrEditedFeatureByIndex(id.m_index);
  feature->ParseGeometry(FeatureType::BEST_GEOMETRY);
  if (feature->GetFeatureType() == feature::EGeomType::GEOM_AREA)
    feature->ParseTriangles(FeatureType::BEST_GEOMETRY);
  CHECK(feature, ());
  return feature;
}

// Testing
Storage::SettingResult Storage::SetUGCUpdateForTesting(FeatureID const & id,
                                                       v0::UGCUpdate const & ugc)
{
  auto const feature = GetFeature(id);
  return SetGenericUGCUpdate(m_UGCIndexes, m_numberOfDeleted, id, ugc,
                             *feature, Version::V0);
}

}  // namespace ugc
