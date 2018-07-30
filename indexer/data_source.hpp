#pragma once

#include "indexer/cell_id.hpp"
#include "indexer/feature.hpp"
#include "indexer/feature_covering.hpp"
#include "indexer/features_offsets_table.hpp"
#include "indexer/feature_source.hpp"
#include "indexer/features_vector.hpp"
#include "indexer/mwm_set.hpp"
#include "indexer/scale_index.hpp"
#include "indexer/unique_index.hpp"

#include "coding/file_container.hpp"

#include "base/macros.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "defines.hpp"

class DataSource : public MwmSet
{
public:
  using FeatureCallback = std::function<void(FeatureType &)>;
  using FeatureIdCallback = std::function<void(FeatureID const &)>;

  ~DataSource() override = default;

  /// Registers a new map.
  std::pair<MwmId, RegResult> RegisterMap(platform::LocalCountryFile const & localFile);

  /// Deregisters a map from internal records.
  ///
  /// \param countryFile A countryFile denoting a map to be deregistered.
  /// \return True if the map was successfully deregistered. If map is locked
  ///         now, returns false.
  bool DeregisterMap(platform::CountryFile const & countryFile);

  void ForEachFeatureIDInRect(FeatureIdCallback const & f, m2::RectD const & rect, int scale) const;
  void ForEachInRect(FeatureCallback const & f, m2::RectD const & rect, int scale) const;
  void ForEachInScale(FeatureCallback const & f, int scale) const;
  void ForEachInRectForMWM(FeatureCallback const & f, m2::RectD const & rect, int scale,
                           MwmId const & id) const;
  // "features" must be sorted using FeatureID::operator< as predicate.
  void ReadFeatures(FeatureCallback const & fn, std::vector<FeatureID> const & features) const;

  void ReadFeature(FeatureCallback const & fn, FeatureID const & feature) const
  {
    return ReadFeatures(fn, {feature});
  }

protected:
  using ReaderCallback = std::function<void(MwmSet::MwmHandle const & handle,
                                            covering::CoveringGetter & cov, int scale)>;

  explicit DataSource(std::unique_ptr<FeatureSourceFactory> factory) : m_factory(std::move(factory)) {}

  void ForEachInIntervals(ReaderCallback const & fn, covering::CoveringMode mode,
                          m2::RectD const & rect, int scale) const;

  /// MwmSet overrides:
  std::unique_ptr<MwmInfo> CreateInfo(platform::LocalCountryFile const & localFile) const override;
  std::unique_ptr<MwmValueBase> CreateValue(MwmInfo & info) const override;

private:
  friend class FeaturesLoaderGuard;

  std::unique_ptr<FeatureSourceFactory> m_factory;
};

// DataSource which operates with features from mwm file and does not support features creation
// deletion or modification.
class FrozenDataSource : public DataSource
{
public:
  FrozenDataSource() : DataSource(std::make_unique<FeatureSourceFactory>()) {}
};

/// Guard for loading features from particular MWM by demand.
/// @note This guard is suitable when mwm is loaded.
class FeaturesLoaderGuard
{
public:
  FeaturesLoaderGuard(DataSource const & dataSource, DataSource::MwmId const & id)
    : m_handle(dataSource.GetMwmHandleById(id)), m_source((*dataSource.m_factory)(m_handle))
  {
  }

  MwmSet::MwmId const & GetId() const { return m_handle.GetId(); }
  std::string GetCountryFileName() const;
  bool IsWorld() const;
  std::unique_ptr<FeatureType> GetOriginalFeatureByIndex(uint32_t index) const;
  std::unique_ptr<FeatureType> GetOriginalOrEditedFeatureByIndex(uint32_t index) const;
  /// Everyone, except Editor core, should use this method.
  WARN_UNUSED_RESULT bool GetFeatureByIndex(uint32_t index, FeatureType & ft) const;
  /// Editor core only method, to get 'untouched', original version of feature.
  WARN_UNUSED_RESULT bool GetOriginalFeatureByIndex(uint32_t index, FeatureType & ft) const;
  size_t GetNumFeatures() const { return m_source->GetNumFeatures(); }

private:
  DataSource::MwmHandle m_handle;
  std::unique_ptr<FeatureSource> m_source;
};
