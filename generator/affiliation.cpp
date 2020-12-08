#include "generator/affiliation.hpp"

#include "platform/platform.hpp"

#include "geometry/mercator.hpp"

#include "base/logging.hpp"
#include "base/thread_pool_computational.hpp"

#include <cmath>
#include <functional>
#include <unordered_map>
#include <unordered_set>

#include "3party/boost/boost/geometry.hpp"
#include "3party/boost/boost/geometry/geometries/register/point.hpp"
#include "3party/boost/boost/geometry/geometries/register/ring.hpp"

BOOST_GEOMETRY_REGISTER_POINT_2D(m2::PointD, double, boost::geometry::cs::cartesian, x, y);
BOOST_GEOMETRY_REGISTER_RING(std::vector<m2::PointD>);

namespace bgi = boost::geometry::index;

namespace
{
using namespace feature;

template <typename T>
struct RemoveCvref
{
  typedef std::remove_cv_t<std::remove_reference_t<T>> type;
};

template <typename T>
using RemoveCvrefT = typename RemoveCvref<T>::type;

template <typename T>
m2::RectD GetLimitRect(T && t)
{
  using Type = RemoveCvrefT<T>;
  if constexpr(std::is_same_v<Type, FeatureBuilder>)
    return t.GetLimitRect();
  if constexpr (std::is_same_v<Type, m2::RectD>)
    return m2::RectD(t);
  if constexpr(std::is_same_v<Type, m2::PointD>)
    return m2::RectD(t, t);

  UNREACHABLE();
}

template <typename T, typename F>
bool ForAnyPoint(T && t, F && f)
{
  using Type = RemoveCvrefT<T>;
  if constexpr(std::is_same_v<Type, FeatureBuilder>)
    return t.ForAnyPoint(std::forward<F>(f));
  if constexpr (std::is_same_v<Type, m2::RectD>)
    return base::AnyOf(
        std::vector<m2::PointD>{t.LeftTop(), t.LeftBottom(), t.RightBottom(), t.RightTop()},
        std::forward<F>(f));
  if constexpr(std::is_same_v<Type, m2::PointD>)
    return f(std::forward<T>(t));

  UNREACHABLE();
}

template <typename T, typename F>
void ForEachPoint(T && t, F && f)
{
  using Type = RemoveCvrefT<T>;
  if constexpr(std::is_same_v<Type, FeatureBuilder>)
    t.ForEachPoint(std::forward<F>(f));
  else if constexpr (std::is_same_v<Type, m2::RectD>)
    t.ForEachCorner(std::forward<F>(f));
  else if constexpr(std::is_same_v<Type, m2::PointD>)
    f(std::forward<T>(t));
  else
    UNREACHABLE();
}

// An implementation for CountriesFilesAffiliation class.
template <typename T>
std::vector<CountryPolygonsPtr> GetAffiliations(
    T && t, borders::CountryPolygonsCollection const & countryPolygonsTree,
    bool haveBordersForWholeWorld)
{
  std::vector<CountryPolygonsPtr> countries;
  std::vector<CountryPolygonsPtr> countriesContainer;
  countryPolygonsTree.ForEachCountryInRect(GetLimitRect(t),
                                           [&](borders::CountryPolygons const & countryPolygons) {
                                             countriesContainer.emplace_back(&countryPolygons);
                                           });

  // todo(m.andrianov): We need to explore this optimization better. There is a hypothesis: some
  // elements belong to a rectangle, but do not belong to the exact boundary.
  if (haveBordersForWholeWorld && countriesContainer.size() == 1)
  {
    auto const * countryPolygons = countriesContainer.front();
    countries.emplace_back(countryPolygons);
    return countries;
  }

  for (auto const * countryPolygons : countriesContainer)
  {
    auto const need =
        ForAnyPoint(t, [&](auto const & point) { return countryPolygons->Contains(point); });

    if (need)
      countries.emplace_back(countryPolygons);
  }

  return countries;
}

// An implementation for CountriesFilesIndexAffiliation class.
using IndexSharedPtr = std::shared_ptr<CountriesFilesIndexAffiliation::Tree>;

CountriesFilesIndexAffiliation::Box MakeBox(m2::RectD const & rect)
{
  return {rect.LeftBottom(), rect.RightTop()};
}

CountryPolygonsPtr IsOneCountryForLimitRect(m2::RectD const & limitRect,
                                            IndexSharedPtr const & index)
{
  auto const bbox = MakeBox(limitRect);
  CountryPolygonsPtr country = nullptr;
  for (auto it = bgi::qbegin(*index, bgi::covers(bbox)), end = bgi::qend(*index); it != end; ++it)
  {
    for (auto const * c : it->second)
    {
      if (!country)
        country = c;
      else if (country != c)
        return nullptr;
    }
  }

  return country;
}

template <typename T>
std::vector<CountryPolygonsPtr> GetHonestAffiliations(T && t, IndexSharedPtr const & index)
{
  std::unordered_set<CountryPolygonsPtr> countires;
  ForEachPoint(t, [&](auto const & point) {
    for (auto it = bgi::qbegin(*index, bgi::covers(point)), end = bgi::qend(*index); it != end;
         ++it)
    {
      if (it->second.size() == 1)
      {
        auto const * cp = it->second.front();
        countires.insert(cp);
      }
      else
      {
        for (auto const * cp : it->second)
        {
          if (cp->Contains(point))
            countires.insert(cp);
        }
      }
    }
  });

  return std::vector<CountryPolygonsPtr>(std::cbegin(countires), std::cend(countires));
}

template <typename T>
std::vector<CountryPolygonsPtr> GetAffiliations(T && t, IndexSharedPtr const & index)
{
  auto const oneCountry = IsOneCountryForLimitRect(GetLimitRect(t), index);
  return oneCountry ? std::vector<CountryPolygonsPtr>{oneCountry} : GetHonestAffiliations(t, index);
}
}  // namespace

namespace feature
{
CountriesFilesAffiliation::CountriesFilesAffiliation(std::string const & borderPath,
                                                     bool haveBordersForWholeWorld)
  : m_countryPolygonsTree(borders::GetOrCreateCountryPolygonsTree(borderPath))
  , m_borderPath(borderPath)
  , m_haveBordersForWholeWorld(haveBordersForWholeWorld)
{
}

std::vector<CountryPolygonsPtr> CountriesFilesAffiliation::GetAffiliations(
    FeatureBuilder const & fb) const
{
  return ::GetAffiliations(fb, m_countryPolygonsTree, m_haveBordersForWholeWorld);
}

std::vector<CountryPolygonsPtr> CountriesFilesAffiliation::GetAffiliations(
    m2::RectD const & rect) const
{
  std::vector<CountryPolygonsPtr> countries;
  std::vector<CountryPolygonsPtr> countriesContainer;
  m_countryPolygonsTree.ForEachCountryInRect(rect,
                                             [&](borders::CountryPolygons const & countryPolygons) {
                                               countriesContainer.emplace_back(&countryPolygons);
                                             });

  // todo(m.andrianov): We need to explore this optimization better. There is a hypothesis: some
  // elements belong to a rectangle, but do not belong to the exact boundary.
  if (m_haveBordersForWholeWorld && countriesContainer.size() == 1)
  {
    auto const * countryPolygons = countriesContainer.front();
    countries.emplace_back(countryPolygons);
    return countries;
  }

  for (auto const * countryPolygons : countriesContainer)
  {
    countryPolygons->ForEachInRect(rect, [&](auto const & p) {
      if (boost::geometry::intersects(p.Data(), MakeBox(rect)))
        countries.emplace_back(countryPolygons);
    });
  }

  return countries;
}

std::vector<CountryPolygonsPtr> CountriesFilesAffiliation::GetAffiliations(
    m2::PointD const & point) const
{
  return ::GetAffiliations(point, m_countryPolygonsTree, m_haveBordersForWholeWorld);
}

bool CountriesFilesAffiliation::HasCountryByName(std::string const & name) const
{
  return m_countryPolygonsTree.HasRegionByName(name);
}

CountriesFilesIndexAffiliation::CountriesFilesIndexAffiliation(std::string const & borderPath,
                                                               bool haveBordersForWholeWorld)
  : CountriesFilesAffiliation(borderPath, haveBordersForWholeWorld)
{
  auto affiliaction =
      std::make_unique<CountriesFilesAffiliation>(borderPath, haveBordersForWholeWorld);
  auto const steps = haveBordersForWholeWorld ? std::vector<double>{1.0, 0.5, 0.2, 0.1, 0.05}
                                              : std::vector<double>{0.2};
  for (auto const & step : steps)
  {
    LOG(LINFO, ("Building index for step", step));
    auto net =
        generator::cells_merger::MakeNet(step, mercator::Bounds::kMinX, mercator::Bounds::kMinY,
                                         mercator::Bounds::kMaxX, mercator::Bounds::kMaxY);
    affiliaction = BuildIndex(borderPath, haveBordersForWholeWorld, net, std::move(affiliaction));
  }

  m_index = static_cast<CountriesFilesIndexAffiliation *>(affiliaction.get())->m_index;
}

CountriesFilesIndexAffiliation::CountriesFilesIndexAffiliation(std::string const & borderPath,
                                                               bool haveBordersForWholeWorld,
                                                               std::shared_ptr<Tree> const & tree)
  : CountriesFilesAffiliation(borderPath, haveBordersForWholeWorld), m_index(tree)
{
}

std::vector<CountryPolygonsPtr> CountriesFilesIndexAffiliation::GetAffiliations(
    FeatureBuilder const & fb) const
{
  return ::GetAffiliations(fb, m_index);
}

std::vector<CountryPolygonsPtr> CountriesFilesIndexAffiliation::GetAffiliations(
    m2::RectD const & rect) const
{
  std::unordered_set<CountryPolygonsPtr> countires;
  auto const box = MakeBox(rect);
  for (auto it = bgi::qbegin(*m_index, bgi::intersects(box)), end = bgi::qend(*m_index); it != end;
       ++it)
  {
    if (it->second.size() == 1)
    {
      auto const * cp = it->second.front();
      countires.insert(cp);
    }
    else
    {
      for (auto const * cp : it->second)
      {
        cp->ForEachInRect(rect, [&](auto const & p) {
          if (boost::geometry::intersects(p.Data(), box))
            countires.insert(cp);
        });
      }
    }
  }

  return std::vector<CountryPolygonsPtr>(std::cbegin(countires), std::cend(countires));
}

std::vector<CountryPolygonsPtr> CountriesFilesIndexAffiliation::GetAffiliations(
    m2::PointD const & point) const
{
  return ::GetAffiliations(point, m_index);
}

// static
std::unique_ptr<CountriesFilesIndexAffiliation> CountriesFilesIndexAffiliation::BuildIndex(
    std::string const & borderPath, bool haveBordersForWholeWorld, std::deque<m2::RectD> & net,
    std::unique_ptr<AffiliationInterface> affiliation)
{
  std::unordered_map<CountryPolygonsPtr, std::deque<m2::RectD>> countriesRects;
  std::deque<Value> treeCells;
  auto const numThreads = GetPlatform().CpuCores();
  base::thread_pool::computational::ThreadPool pool(numThreads);
  auto const step = std::max(net.size() / numThreads, static_cast<size_t>(1));
  std::vector<std::future<std::pair<decltype(countriesRects), decltype(treeCells)>>> futures;
  futures.reserve(numThreads);
  for (size_t l = 0, r = step; l < net.size(); l += step, r += step)
  {
    auto future = pool.Submit([&, l, r]() mutable {
      std::unordered_map<CountryPolygonsPtr, std::deque<m2::RectD>> countriesRects;
      std::deque<Value> treeCells;
      r = std::min(r, net.size());
      for (size_t i = l; i < r; ++i)
      {
        auto const & rect = net[i];
        auto countries = affiliation->GetAffiliations(rect);
        if (countries.size() == 1)
          countriesRects[countries.front()].emplace_back(rect);
        else if (countries.size() > 1)
          treeCells.emplace_back(MakeBox(rect), std::move(countries));
      }
      return std::make_pair(countriesRects, treeCells);
    });

    futures.emplace_back(std::move(future));
  }
  for (auto && future : futures)
  {
    const auto [localCountriesRects, localTreeCells] = future.get();
    std::move(std::begin(localTreeCells), std::end(localTreeCells), std::back_inserter(treeCells));
    for (auto && [country, localRects] : localCountriesRects)
    {
      auto & rects = countriesRects[country];
      std::move(std::begin(localRects), std::end(localRects), std::back_inserter(rects));
    }
  }

  net = {};
  std::mutex treeCellsMutex;
  for (auto & pair : countriesRects)
  {
    pool.SubmitWork([&, countryPtr{pair.first}, rects{std::move(pair.second)}]() mutable {
      generator::cells_merger::CellsMerger merger(std::move(rects));
      auto const merged = merger.Merge();
      for (auto const & rect : merged)
      {
        std::vector<CountryPolygonsPtr> interCountries{countryPtr};
        std::lock_guard<std::mutex> lock(treeCellsMutex);
        treeCells.emplace_back(MakeBox(rect), std::move(interCountries));
      }
    });
  }

  pool.WaitingStop();
  return std::unique_ptr<CountriesFilesIndexAffiliation>(new CountriesFilesIndexAffiliation(
      borderPath, haveBordersForWholeWorld, std::make_shared<Tree>(treeCells)));
}

SingleAffiliation::SingleAffiliation(std::string const & name) : m_polygon(name, {}) {}

std::vector<CountryPolygonsPtr> SingleAffiliation::GetAffiliations(FeatureBuilder const &) const
{
  return {&m_polygon};
}

std::vector<CountryPolygonsPtr> SingleAffiliation::GetAffiliations(m2::RectD const &) const
{
  return {&m_polygon};
}

std::vector<CountryPolygonsPtr> SingleAffiliation::GetAffiliations(m2::PointD const &) const
{
  return {&m_polygon};
}

bool SingleAffiliation::HasCountryByName(std::string const & name) const
{
  return name == m_polygon.GetName();
}
}  // namespace feature
