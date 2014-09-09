#pragma once
#include "coding_params.hpp"
#include "data_header.hpp"

#include "../coding/file_container.hpp"

#include "../std/noncopyable.hpp"


namespace feature
{
  void LoadMapHeader(ModelReaderPtr const & reader, DataHeader & header);

  class LoaderBase;

  /// This info is created once when MWM is added to index for processing.
  class SharedLoadInfo : private noncopyable
  {
    FilesContainerR m_cont;
    DataHeader m_header;

    vector<uint32_t> m_stringOffsets;

    typedef FilesContainerR::ReaderT ReaderT;

    LoaderBase * m_pLoader;
    void CreateLoader();

    void LoadStringOffsets();

    void Init();

  public:
    /// @param[in] fPath Full path of MWM file.
    explicit SharedLoadInfo(string const & fPath);
    explicit SharedLoadInfo(ModelReaderPtr reader);
    ~SharedLoadInfo();

    inline DataHeader const & GetHeader() const { return m_header; }
    inline string const & GetFileName() const { return m_cont.GetFileName(); }

    ReaderT GetStringReader() const;
    ReaderT GetDataReader() const;
    ReaderT GetGeometryReader(int ind) const;
    ReaderT GetTrianglesReader(int ind) const;

    ReaderT GetGeometryIndexReader() const;
    ReaderT GetSearchIndexReader() const;
    bool IsSearchIndexExists() const;

    string GetString(uint32_t ind) const;

    LoaderBase * GetLoader() const { return m_pLoader; }

    inline serial::CodingParams const & GetDefCodingParams() const
    {
      return m_header.GetDefCodingParams();
    }
    inline serial::CodingParams GetCodingParams(int scaleIndex) const
    {
      return m_header.GetCodingParams(scaleIndex);
    }

    inline int GetScalesCount() const { return static_cast<int>(m_header.GetScalesCount()); }
    inline int GetScale(int i) const { return m_header.GetScale(i); }
    inline int GetLastScale() const { return m_header.GetLastScale(); }
  };
}

class IntervalIndexIFace;

class IndexFactory
{
  feature::DataHeader const & m_header;
public:
  IndexFactory(feature::DataHeader const & header) : m_header(header) {}
  IntervalIndexIFace * CreateIndex(ModelReaderPtr reader);
};
