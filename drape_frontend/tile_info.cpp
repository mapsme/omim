#include "drape_frontend/drape_measurer.hpp"
#include "drape_frontend/engine_context.hpp"
#include "drape_frontend/map_data_provider.hpp"
#include "drape_frontend/rule_drawer.hpp"
#include "drape_frontend/stylist.hpp"
#include "drape_frontend/tile_info.hpp"

#include "indexer/scales.hpp"

#include "platform/preferred_languages.hpp"

#include "base/scope_guard.hpp"
#include "base/logging.hpp"

#include <functional>

namespace df
{

TileInfo::TileInfo(drape_ptr<EngineContext> && engineContext,
                   CustomSymbolsContextWeakPtr customSymbolsContext)
  : m_context(move(engineContext))
  , m_customSymbolsContext(customSymbolsContext)
  , m_is3dBuildings(false)
  , m_trafficEnabled(false)
  , m_isCanceled(false)
{}

m2::RectD TileInfo::GetGlobalRect() const
{
  return GetTileKey().GetGlobalRect();
}

void TileInfo::ReadFeatureIndex(MapDataProvider const & model)
{
  if (DoNeedReadIndex())
  {
    CheckCanceled();

    size_t const kAverageFeaturesCount = 256;
    m_featureInfo.reserve(kAverageFeaturesCount);

#ifdef DEBUG
    set<MwmSet::MwmId> existing;
    MwmSet::MwmId lastMwm;
    model.ReadFeaturesID([this, &existing, &lastMwm](FeatureID const & id)
    {
      if (existing.empty() || lastMwm != id.m_mwmId)
      {
        ASSERT(existing.insert(id.m_mwmId).second, ());
        lastMwm = id.m_mwmId;
      }
#else
    model.ReadFeaturesID([this](FeatureID const & id)
    {
#endif
      m_featureInfo.push_back(id);
    }, GetGlobalRect(), GetZoomLevel());
  }
}

void TileInfo::ReadFeatures(MapDataProvider const & model)
{
#if defined(DRAPE_MEASURER) && defined(TILES_STATISTIC)
  DrapeMeasurer::Instance().StartTileReading();
#endif
  m_context->BeginReadTile();

  // Reading can be interrupted by exception throwing
  MY_SCOPE_GUARD(ReleaseReadTile, std::bind(&EngineContext::EndReadTile, m_context.get()));

  ReadFeatureIndex(model);
  CheckCanceled();

  if (!m_featureInfo.empty())
  {
    auto const deviceLang = StringUtf8Multilang::GetLangIndex(languages::GetCurrentNorm());
    RuleDrawer drawer(std::bind(&TileInfo::InitStylist, this, deviceLang, std::placeholders::_1, std::placeholders::_2),
                      std::bind(&TileInfo::IsCancelled, this),
                      model.m_isCountryLoadedByName,
                      make_ref(m_context), m_customSymbolsContext.lock(),
                      m_is3dBuildings, m_trafficEnabled);
    model.ReadFeatures(std::bind<void>(std::ref(drawer), std::placeholders::_1), m_featureInfo);
  }
#if defined(DRAPE_MEASURER) && defined(TILES_STATISTIC)
  DrapeMeasurer::Instance().EndTileReading();
#endif
}

void TileInfo::Cancel()
{
  m_isCanceled = true;
}

bool TileInfo::IsCancelled() const
{
  return m_isCanceled;
}

void TileInfo::InitStylist(int8_t deviceLang, FeatureType const & f, Stylist & s)
{
  CheckCanceled();
  df::InitStylist(f, deviceLang, m_context->GetTileKey().m_zoomLevel, m_is3dBuildings, s);
}

bool TileInfo::DoNeedReadIndex() const
{
  return m_featureInfo.empty();
}

void TileInfo::CheckCanceled() const
{
  if (m_isCanceled)
    MYTHROW(ReadCanceledException, ());
}

int TileInfo::GetZoomLevel() const
{
  return ClipTileZoomByMaxDataZoom(m_context->GetTileKey().m_zoomLevel);
}

} // namespace df
