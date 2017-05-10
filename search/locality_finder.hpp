#pragma once

#include "indexer/mwm_set.hpp"
#include "indexer/rank_table.hpp"

#include "geometry/point2d.hpp"
#include "geometry/rect2d.hpp"
#include "geometry/tree4d.hpp"

#include "base/macros.hpp"

#include <memory>
#include <unordered_set>

class Index;

namespace search
{
class VillagesCache;

struct LocalityItem
{
  LocalityItem(std::string const & name, m2::PointD const & center, uint64_t population);

  std::string m_name;
  m2::PointD m_center;
  uint64_t m_population;
};

std::string DebugPrint(LocalityItem const & item);

class LocalitySelector
{
public:
  LocalitySelector(std::string & name, m2::PointD const & p);

  void operator()(LocalityItem const & item);

private:
  std::string & m_name;
  m2::PointD m_p;
  double m_bestScore;
};

class LocalityFinder
{
public:
  class Holder
  {
   public:
    Holder(double radiusMeters);

    bool IsCovered(m2::RectD const & rect) const;
    void SetCovered(m2::PointD const & p);

    void Add(LocalityItem const & item);
    void ForEachInVicinity(m2::RectD const & rect, LocalitySelector & selector) const;

    m2::RectD GetRect(m2::PointD const & p) const;
    m2::RectD GetDRect(m2::PointD const & p) const;

    void Clear();

   private:

    double const m_radiusMeters;
    m4::Tree<bool> m_coverage;
    m4::Tree<LocalityItem> m_localities;

    DISALLOW_COPY_AND_MOVE(Holder);
  };

  LocalityFinder(Index const & index, VillagesCache & villagesCache);

  void SetLanguage(int8_t lang);
  void GetLocality(m2::PointD const & p, std::string & name);
  void ClearCache();

private:
  void LoadVicinity(m2::PointD const & p, bool loadCities, bool loadVillages);
  void UpdateMaps();

  Index const & m_index;
  VillagesCache & m_villagesCache;
  int8_t m_lang;

  Holder m_cities;
  Holder m_villages;

  m4::Tree<MwmSet::MwmId> m_maps;
  MwmSet::MwmId m_worldId;
  bool m_mapsLoaded;

  std::unique_ptr<RankTable> m_ranks;

  map<MwmSet::MwmId, std::unordered_set<uint32_t>> m_loadedIds;
};
}  // namespace search
