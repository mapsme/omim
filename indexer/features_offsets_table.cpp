#include "indexer/features_offsets_table.hpp"
#include "indexer/features_vector.hpp"

#include "platform/local_country_file.hpp"
#include "platform/local_country_file_utils.hpp"
#include "platform/platform.hpp"

#include "coding/file_container.hpp"
#include "coding/internal/file_data.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"
#include "base/scope_guard.hpp"

#include <string>


using namespace platform;

namespace feature
{
  void FeaturesOffsetsTable::Builder::PushOffset(uint32_t const offset)
  {
    ASSERT(m_offsets.empty() || m_offsets.back() < offset, ());
    m_offsets.push_back(offset);
  }

  FeaturesOffsetsTable::FeaturesOffsetsTable(succinct::elias_fano::elias_fano_builder & builder)
      : m_table(&builder)
  {
  }

  FeaturesOffsetsTable::FeaturesOffsetsTable(std::string const & filePath)
  {
    m_pReader.reset(new MmapReader(filePath));
    succinct::mapper::map(m_table, reinterpret_cast<char const *>(m_pReader->Data()));
  }

  // static
  std::unique_ptr<FeaturesOffsetsTable> FeaturesOffsetsTable::Build(Builder & builder)
  {
    std::vector<uint32_t> const & offsets = builder.m_offsets;

    size_t const numOffsets = offsets.size();
    uint32_t const maxOffset = offsets.empty() ? 0 : offsets.back();

    succinct::elias_fano::elias_fano_builder elias_fano_builder(maxOffset, numOffsets);
    for (uint32_t offset : offsets)
      elias_fano_builder.push_back(offset);

    return std::unique_ptr<FeaturesOffsetsTable>(new FeaturesOffsetsTable(elias_fano_builder));
  }

  // static
  std::unique_ptr<FeaturesOffsetsTable> FeaturesOffsetsTable::LoadImpl(std::string const & filePath)
  {
    return std::unique_ptr<FeaturesOffsetsTable>(new FeaturesOffsetsTable(filePath));
  }

  // static
  std::unique_ptr<FeaturesOffsetsTable> FeaturesOffsetsTable::Load(std::string const & filePath)
  {
    if (!GetPlatform().IsFileExistsByFullPath(filePath))
      return std::unique_ptr<FeaturesOffsetsTable>();
    return LoadImpl(filePath);
  }

  // static
  std::unique_ptr<FeaturesOffsetsTable> FeaturesOffsetsTable::Load(FilesContainerR const & cont)
  {
    std::unique_ptr<FeaturesOffsetsTable> table(new FeaturesOffsetsTable());

    table->m_file.Open(cont.GetFileName());
    auto p = cont.GetAbsoluteOffsetAndSize(FEATURE_OFFSETS_FILE_TAG);
    table->m_handle.Assign(table->m_file.Map(p.first, p.second, FEATURE_OFFSETS_FILE_TAG));

    succinct::mapper::map(table->m_table, table->m_handle.GetData<char>());
    return table;
  }

  // static
  std::unique_ptr<FeaturesOffsetsTable> FeaturesOffsetsTable::CreateImpl(
      platform::LocalCountryFile const & localFile,
      FilesContainerR const & cont, std::string const & storePath)
  {
    LOG(LINFO, ("Creating features offset table file", storePath));

    CountryIndexes::PreparePlaceOnDisk(localFile);

    return Build(cont, storePath);
  }

  std::unique_ptr<FeaturesOffsetsTable> FeaturesOffsetsTable::Build(FilesContainerR const & cont,
                                                               std::string const & storePath)
  {
    Builder builder;
    FeaturesVector::ForEachOffset(cont.GetReader(DATA_FILE_TAG), [&builder] (uint32_t offset)
    {
      builder.PushOffset(offset);
    });

    std::unique_ptr<FeaturesOffsetsTable> table(Build(builder));
    table->Save(storePath);
    return table;
  }

  // static
  std::unique_ptr<FeaturesOffsetsTable> FeaturesOffsetsTable::CreateIfNotExistsAndLoad(
      LocalCountryFile const & localFile, FilesContainerR const & cont)
  {
    std::string const offsetsFilePath = CountryIndexes::GetPath(localFile, CountryIndexes::Index::Offsets);

    if (Platform::IsFileExistsByFullPath(offsetsFilePath))
      return LoadImpl(offsetsFilePath);

    return CreateImpl(localFile, cont, offsetsFilePath);
  }

  // static
  std::unique_ptr<FeaturesOffsetsTable> FeaturesOffsetsTable::CreateIfNotExistsAndLoad(
      LocalCountryFile const & localFile)
  {
    std::string const offsetsFilePath = CountryIndexes::GetPath(localFile, CountryIndexes::Index::Offsets);

    if (Platform::IsFileExistsByFullPath(offsetsFilePath))
      return LoadImpl(offsetsFilePath);

    return CreateImpl(localFile, FilesContainerR(localFile.GetPath(MapOptions::Map)), offsetsFilePath);
  }

  // static
  std::unique_ptr<FeaturesOffsetsTable> FeaturesOffsetsTable::CreateIfNotExistsAndLoad(FilesContainerR const & cont)
  {
    return CreateIfNotExistsAndLoad(LocalCountryFile::MakeTemporary(cont.GetFileName()), cont);
  }

  void FeaturesOffsetsTable::Save(std::string const & filePath)
  {
    LOG(LINFO, ("Saving features offsets table to ", filePath));
    std::string const fileNameTmp = filePath + EXTENSION_TMP;
    succinct::mapper::freeze(m_table, fileNameTmp.c_str());
    my::RenameFileX(fileNameTmp, filePath);
  }

  uint32_t FeaturesOffsetsTable::GetFeatureOffset(size_t index) const
  {
    ASSERT_LESS(index, size(), ("Index out of bounds", index, size()));
    return static_cast<uint32_t>(m_table.select(index));
  }

  size_t FeaturesOffsetsTable::GetFeatureIndexbyOffset(uint32_t offset) const
  {
    ASSERT_GREATER(size(), 0, ("We must not ask empty table"));
    ASSERT_LESS_OR_EQUAL(offset, m_table.select(size() - 1), ("Offset out of bounds", offset,
                                                     m_table.select(size() - 1)));
    ASSERT_GREATER_OR_EQUAL(offset, m_table.select(0), ("Offset out of bounds", offset,
                                                        m_table.select(size() - 1)));
    //Binary search in elias_fano list
    size_t leftBound = 0, rightBound = size();
    while (leftBound + 1 < rightBound) {
      size_t middle = leftBound + (rightBound - leftBound) / 2;
      if (m_table.select(middle) <= offset)
        leftBound = middle;
      else
        rightBound = middle;
    }
    ASSERT_EQUAL(offset, m_table.select(leftBound), ("Can't find offset", offset, "in the table"));
    return leftBound;
  }

  bool BuildOffsetsTable(std::string const & filePath)
  {
    try
    {
      std::string const destPath = filePath + ".offsets";
      MY_SCOPE_GUARD(fileDeleter, bind(FileWriter::DeleteFileX, destPath));

      (void)feature::FeaturesOffsetsTable::Build(FilesContainerR(filePath), destPath);
      FilesContainerW(filePath, FileWriter::OP_WRITE_EXISTING).Write(destPath, FEATURE_OFFSETS_FILE_TAG);
      return true;
    }
    catch (RootException const & ex)
    {
      LOG(LERROR, ("Generating offsets table failed for", filePath, "reason", ex.Msg()));
      return false;
    }
  }

}  // namespace feature
