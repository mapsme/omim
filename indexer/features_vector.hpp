#pragma once
#include "feature.hpp"
#include "feature_loader_base.hpp"

#include "../coding/var_record_reader.hpp"


/// Note! This class is NOT Thread-Safe.
/// You should have separate instance of Vector for every thread.
class FeaturesVector
{
public:
  FeaturesVector(feature::SharedLoadInfo const & loadInfo)
    : m_loadInfo(loadInfo), m_recordReader(loadInfo.GetDataReader(), 256)
  {
  }

  void Get(uint64_t pos, FeatureType & ft) const
  {
    uint32_t offset;
    m_recordReader.ReadRecord(pos, m_buffer, offset);

    ft.Deserialize(m_loadInfo.GetLoader(), &m_buffer[offset]);
  }

  template <class ToDo> void ForEachOffset(ToDo toDo) const
  {
    m_recordReader.ForEachRecord(DoGetFeatures<ToDo>(m_loadInfo, toDo));
  }

private:
  template <class ToDo> class DoGetFeatures
  {
    feature::SharedLoadInfo const & m_loadInfo;
    ToDo & m_toDo;

  public:
    DoGetFeatures(feature::SharedLoadInfo const & loadInfo, ToDo & toDo)
      : m_loadInfo(loadInfo), m_toDo(toDo)
    {
    }

    void operator() (uint32_t pos, char const * data, uint32_t /*size*/) const
    {
      FeatureType ft;
      ft.Deserialize(m_loadInfo.GetLoader(), data);

      m_toDo(ft, pos);
    }
  };

private:
  feature::SharedLoadInfo const & m_loadInfo;
  VarRecordReader<FilesContainerR::ReaderT, &VarRecordSizeReaderVarint> m_recordReader;
  mutable vector<char> m_buffer;
};
