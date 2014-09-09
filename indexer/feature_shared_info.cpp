#include "feature_shared_info.hpp"
#include "feature_loader.hpp"
#include "old/feature_loader_101.hpp"
#include "feature_impl.hpp"

#include "interval_index.hpp"
#include "old/interval_index_101.hpp"

#include "mwm_version.hpp"

#include "../defines.hpp"


namespace feature
{

//////////////////////////////////////////////////////////////////////////////////////////////
/// Loading of MWM header.
//////////////////////////////////////////////////////////////////////////////////////////////

void LoadMapHeader(FilesContainerR const & cont, DataHeader & header)
{
  ModelReaderPtr headerReader = cont.GetReader(HEADER_FILE_TAG);

  if (!cont.IsReaderExist(VERSION_FILE_TAG))
    header.LoadVer1(headerReader);
  else
  {
    ModelReaderPtr verReader = cont.GetReader(VERSION_FILE_TAG);
    header.Load(headerReader, static_cast<DataHeader::Version>(ver::ReadVersion(verReader)));
  }
}

void LoadMapHeader(ModelReaderPtr const & reader, DataHeader & header)
{
  LoadMapHeader(FilesContainerR(reader), header);
}

//////////////////////////////////////////////////////////////////////////////////////////////
/// SharedLoadInfo implementation.
//////////////////////////////////////////////////////////////////////////////////////////////

void SharedLoadInfo::Init()
{
  LoadMapHeader(m_cont, m_header);

  CreateLoader();

  LoadStringOffsets();
}

SharedLoadInfo::SharedLoadInfo(string const & fPath)
  : m_cont(fPath)
{
  Init();
}

SharedLoadInfo::SharedLoadInfo(ModelReaderPtr reader)
  : m_cont(reader)
{
  Init();
}

SharedLoadInfo::~SharedLoadInfo()
{
  delete m_pLoader;
}

void SharedLoadInfo::LoadStringOffsets()
{
  try
  {
    // fill offsets of external strings
    ReaderSource<ReaderT> src(GetStringReader());
    while (src.Size() > 0)
    {
      m_stringOffsets.push_back(src.Pos());
      src.Skip(ReadVarUint<uint32_t>(src));
    }
  }
  catch (RootException const & ex)
  {
    LOG(LINFO, ("Can't load external strings:", m_cont.GetFileName(), ex.Msg()));
    m_stringOffsets.clear();
  }
}

SharedLoadInfo::ReaderT SharedLoadInfo::GetStringReader() const
{
  return m_cont.GetReader(STRINGS_FILE_TAG);
}

SharedLoadInfo::ReaderT SharedLoadInfo::GetDataReader() const
{
  return m_cont.GetReader(DATA_FILE_TAG);
}

SharedLoadInfo::ReaderT SharedLoadInfo::GetGeometryReader(int ind) const
{
  return m_cont.GetReader(GetTagForIndex(GEOMETRY_FILE_TAG, ind));
}

SharedLoadInfo::ReaderT SharedLoadInfo::GetTrianglesReader(int ind) const
{
  return m_cont.GetReader(GetTagForIndex(TRIANGLE_FILE_TAG, ind));
}

SharedLoadInfo::ReaderT SharedLoadInfo::GetGeometryIndexReader() const
{
  return m_cont.GetReader(INDEX_FILE_TAG);
}

SharedLoadInfo::ReaderT SharedLoadInfo::GetSearchIndexReader() const
{
  return m_cont.GetReader(SEARCH_INDEX_FILE_TAG);
}

bool SharedLoadInfo::IsSearchIndexExists() const
{
  return m_cont.IsReaderExist(SEARCH_INDEX_FILE_TAG);
}

string SharedLoadInfo::GetString(uint32_t ind) const
{
  ASSERT_LESS(ind, MAX_EXTERNAL_STRINGS, ());

  ReaderSource<ReaderT> src(GetStringReader());
  src.Skip(m_stringOffsets[ind]);
  uint32_t const count = ReadVarUint<uint32_t>(src);

  string str;
  str.resize(count);
  src.Read(&str[0], count);
  return str;
}

void SharedLoadInfo::CreateLoader()
{
  switch (m_header.GetVersion())
  {
  case DataHeader::v1:
    m_pLoader = new old_101::feature::LoaderImpl(*this);
    break;

  default:
    m_pLoader = new LoaderCurrent(*this);
    break;
  }
}

}


IntervalIndexIFace * IndexFactory::CreateIndex(ModelReaderPtr reader)
{
  IntervalIndexIFace * p;

  switch (m_header.GetVersion())
  {
  case feature::DataHeader::v1:
    p = new old_101::IntervalIndex<uint32_t, ModelReaderPtr>(reader);
    break;

  default:
    p = new IntervalIndex<ModelReaderPtr>(reader);
    break;
  }

  return p;
}
