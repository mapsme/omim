#pragma once

#include "drape_frontend/custom_symbol.hpp"
#include "drape_frontend/engine_context.hpp"
#include "drape_frontend/tile_key.hpp"

#include "indexer/feature_decl.hpp"

#include "base/exception.hpp"

#include <atomic>
#include <mutex>
#include <boost/noncopyable.hpp>
#include <vector>

class FeatureType;

namespace df
{

class MapDataProvider;
class Stylist;

class TileInfo : private boost::noncopyable
{
public:
  DECLARE_EXCEPTION(ReadCanceledException, RootException);

  TileInfo(drape_ptr<EngineContext> && engineContext,
           CustomSymbolsContextWeakPtr customSymbolsContext);

  void ReadFeatures(MapDataProvider const & model);
  void Cancel();
  bool IsCancelled() const;

  void Set3dBuildings(bool buildings3d) { m_is3dBuildings = buildings3d; }
  bool Get3dBuildings() const { return m_is3dBuildings; }

  void SetTrafficEnabled(bool trafficEnabled) { m_trafficEnabled = trafficEnabled; }
  bool GetTrafficEnabled() const { return m_trafficEnabled; }

  m2::RectD GetGlobalRect() const;
  TileKey const & GetTileKey() const { return m_context->GetTileKey(); }
  bool operator <(TileInfo const & other) const { return GetTileKey() < other.GetTileKey(); }

private:
  void ReadFeatureIndex(MapDataProvider const & model);
  void InitStylist(int8_t deviceLang, FeatureType const & f, Stylist & s);
  void CheckCanceled() const;
  bool DoNeedReadIndex() const;

  int GetZoomLevel() const;

private:
  drape_ptr<EngineContext> m_context;
  CustomSymbolsContextWeakPtr m_customSymbolsContext;
  std::vector<FeatureID> m_featureInfo;
  bool m_is3dBuildings;
  bool m_trafficEnabled;

  std::atomic<bool> m_isCanceled;
};

} // namespace df
