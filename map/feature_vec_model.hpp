#pragma once

#include "indexer/data_header.hpp"
#include "indexer/index.hpp"
#include "indexer/mwm_set.hpp"

#include "geometry/rect2d.hpp"
#include "geometry/point2d.hpp"

#include "coding/reader.hpp"
#include "coding/buffer_reader.hpp"

#include "base/macros.hpp"

namespace model
{
//#define USE_BUFFER_READER

class FeaturesFetcher : public MwmSet::Observer
  {
  public:
#ifdef USE_BUFFER_READER
    typedef BufferReader ReaderT;
#else
    typedef ModelReaderPtr ReaderT;
#endif

    typedef function<void(platform::LocalCountryFile const &)> TMapDeregisteredCallback;

  private:
    m2::RectD m_rect;

    Index m_multiIndex;

    TMapDeregisteredCallback m_onMapDeregistered;

  public:
    FeaturesFetcher();

    virtual ~FeaturesFetcher();

    void InitClassificator();

    inline void SetOnMapDeregisteredCallback(TMapDeregisteredCallback const & callback)
    {
      m_onMapDeregistered = callback;
    }

    /// Registers a new map.
    pair<MwmSet::MwmId, MwmSet::RegResult> RegisterMap(
        platform::LocalCountryFile const & localFile);

    /// Deregisters a map denoted by file from internal records.
    bool DeregisterMap(platform::CountryFile const & countryFile);

    void Clear();

    void ClearCaches();

    inline bool IsLoaded(std::string const & countryFileName) const
    {
      return m_multiIndex.IsLoaded(platform::CountryFile(countryFileName));
    }

    // MwmSet::Observer overrides:
    void OnMapUpdated(platform::LocalCountryFile const & newFile,
                      platform::LocalCountryFile const & oldFile) override;
    void OnMapDeregistered(platform::LocalCountryFile const & localFile) override;

    //bool IsLoaded(m2::PointD const & pt) const;

    /// @name Features enumeration.
    //@{
    template <class ToDo>
    void ForEachFeature(m2::RectD const & rect, ToDo && toDo, int scale) const
    {
      m_multiIndex.ForEachInRect(toDo, rect, scale);
    }

    template <class ToDo>
    void ForEachFeatureID(m2::RectD const & rect, ToDo & toDo, int scale) const
    {
      m_multiIndex.ForEachFeatureIDInRect(toDo, rect, scale);
    }

    template <class ToDo>
    void ReadFeatures(ToDo & toDo, vector<FeatureID> const & features) const
    {
      m_multiIndex.ReadFeatures(toDo, features);
    }
    //@}

    Index const & GetIndex() const { return m_multiIndex; }
    Index & GetIndex() { return m_multiIndex; }
    m2::RectD GetWorldRect() const;
  };
}
