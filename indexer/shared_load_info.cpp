#include "indexer/shared_load_info.hpp"

#include "indexer/feature_impl.hpp"

#include "defines.hpp"

namespace feature
{
SharedLoadInfo::SharedLoadInfo(FilesContainerR const & cont, DataHeader const & header)
  : m_cont(cont), m_header(header)
{
  CHECK_NOT_EQUAL(m_header.GetFormat(), version::Format::v1, ("Old maps format is not supported"));
}

SharedLoadInfo::Reader SharedLoadInfo::GetDataReader(FeaturesTag tag) const
{
  return m_cont.GetReader(GetFeaturesTag(tag, GetMWMFormat()));
}

SharedLoadInfo::Reader SharedLoadInfo::GetMetadataReader() const
{
  return m_cont.GetReader(METADATA_FILE_TAG);
}

SharedLoadInfo::Reader SharedLoadInfo::GetMetadataIndexReader() const
{
  return m_cont.GetReader(METADATA_INDEX_FILE_TAG);
}

SharedLoadInfo::Reader SharedLoadInfo::GetAltitudeReader() const
{
  return m_cont.GetReader(ALTITUDES_FILE_TAG);
}

SharedLoadInfo::Reader SharedLoadInfo::GetGeometryReader(FeaturesTag tag, int ind) const
{
  auto const tagStr = GetGeometryFileTag(tag);
  return m_cont.GetReader(GetTagForIndex(tagStr, ind));
}

SharedLoadInfo::Reader SharedLoadInfo::GetTrianglesReader(FeaturesTag tag, int ind) const
{
  auto const tagStr = GetTrianglesFileTag(tag);
  return m_cont.GetReader(GetTagForIndex(tagStr, ind));
}

std::optional<SharedLoadInfo::Reader> SharedLoadInfo::GetPostcodesReader() const
{
  if (!m_cont.IsExist(POSTCODES_FILE_TAG))
    return {};

  return m_cont.GetReader(POSTCODES_FILE_TAG);
}
}  // namespace feature
