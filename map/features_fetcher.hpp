#pragma once

#include "editor/editable_data_source.hpp"

#include "indexer/data_header.hpp"
#include "indexer/mwm_set.hpp"

#include "geometry/point2d.hpp"
#include "geometry/rect2d.hpp"

#include "coding/buffer_reader.hpp"
#include "coding/reader.hpp"

#include "base/macros.hpp"

#include <functional>
#include <string>
#include <utility>
#include <vector>

class FeaturesFetcher : public MwmSet::Observer
{
public:
  using Reader = ModelReaderPtr;

  using MapDeregisteredCallback = std::function<void(platform::LocalCountryFile const &)>;

  FeaturesFetcher();

  virtual ~FeaturesFetcher();

  void InitClassificator();

  void SetOnMapDeregisteredCallback(MapDeregisteredCallback const & callback)
  {
    m_onMapDeregistered = callback;
  }

  // Registers a new map.
  std::pair<MwmSet::MwmId, MwmSet::RegResult> RegisterMap(
      platform::LocalCountryFile const & localFile);

  // Deregisters a map denoted by file from internal records.
  bool DeregisterMap(platform::CountryFile const & countryFile);

  void Clear();

  void ClearCaches();

  bool IsLoaded(std::string const & countryFileName) const
  {
    return m_dataSource.IsLoaded(platform::CountryFile(countryFileName));
  }

  void ForEachFeature(m2::RectD const & rect, std::function<void(FeatureType &)> const & fn,
                      int scale) const
  {
    m_dataSource.ForEachInRect(fn, rect, scale);
  }

  void ForEachFeatureID(m2::RectD const & rect, std::function<void(FeatureID const &)> const & fn,
                        int scale) const
  {
    m_dataSource.ForEachFeatureIDInRect(fn, rect, scale);
  }

  template <class ToDo>
  void ReadFeatures(ToDo & toDo, std::vector<FeatureID> const & features) const
  {
    m_dataSource.ReadFeatures(toDo, features);
  }

  DataSource const & GetDataSource() const { return m_dataSource; }
  DataSource & GetDataSource() { return m_dataSource; }
  m2::RectD GetWorldRect() const;

  // MwmSet::Observer overrides:
  void OnMapDeregistered(platform::LocalCountryFile const & localFile) override;

private:
  m2::RectD m_rect;

  EditableDataSource m_dataSource;

  MapDeregisteredCallback m_onMapDeregistered;
};
