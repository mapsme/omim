#include "geocoder/geocoder.hpp"

#include "search/house_numbers_matcher.hpp"

#include "indexer/search_string_utils.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"
#include "base/scope_guard.hpp"
#include "base/stl_helpers.hpp"
#include "base/string_utils.hpp"
#include "base/timer.hpp"

#include <algorithm>
#include <set>
#include <utility>

using namespace std;

namespace
{
size_t const kMaxResults = 100;

map<geocoder::Type, double> const kWeight = {
    {geocoder::Type::Country, 10.0},
    {geocoder::Type::Region, 5.0},
    {geocoder::Type::Subregion, 4.0},
    {geocoder::Type::Locality, 3.0},
    {geocoder::Type::Suburb, 3.0},
    {geocoder::Type::Sublocality, 2.0},
    {geocoder::Type::Street, 1.0},
    {geocoder::Type::Building, 0.1},
    {geocoder::Type::Count, 0.0},
};

// todo(@m) This is taken from search/geocoder.hpp. Refactor.
struct ScopedMarkTokens
{
  using Type = geocoder::Type;

  // The range is [l, r).
  ScopedMarkTokens(geocoder::Geocoder::Context & context, Type type, size_t l, size_t r)
    : m_context(context), m_type(type), m_l(l), m_r(r)
  {
    CHECK_LESS_OR_EQUAL(l, r, ());
    CHECK_LESS_OR_EQUAL(r, context.GetNumTokens(), ());

    for (size_t i = m_l; i < m_r; ++i)
      m_context.MarkToken(i, m_type);
  }

  ~ScopedMarkTokens()
  {
    for (size_t i = m_l; i < m_r; ++i)
      m_context.MarkToken(i, Type::Count);
  }

  geocoder::Geocoder::Context & m_context;
  Type const m_type;
  size_t m_l;
  size_t m_r;
};

geocoder::Type NextType(geocoder::Type type)
{
  CHECK_NOT_EQUAL(type, geocoder::Type::Count, ());
  auto t = static_cast<size_t>(type);
  return static_cast<geocoder::Type>(t + 1);
}

bool FindParent(vector<geocoder::Geocoder::Layer> const & layers,
                geocoder::Hierarchy::Entry const & e)
{
  CHECK(!layers.empty(), ());
  auto const & layer = layers.back();
  for (auto const * pe : layer.m_entries)
  {
    // Note that the relationship is somewhat inverted: every ancestor
    // is stored in the address but the nodes have no information
    // about their children.
    if (pe->IsParentTo(e))
      return true;
  }
  return false;
}

strings::UniString MakeHouseNumber(geocoder::Tokens const & tokens)
{
  return strings::JoinStrings(tokens, strings::MakeUniString(""));
}
}  // namespace

namespace geocoder
{
// Geocoder::Context -------------------------------------------------------------------------------
Geocoder::Context::Context(string const & query) : m_beam(kMaxResults)
{
  search::NormalizeAndTokenizeString(query, m_tokens);
  m_tokenTypes.assign(m_tokens.size(), Type::Count);
  m_numUsedTokens = 0;
}

vector<Type> & Geocoder::Context::GetTokenTypes() { return m_tokenTypes; }

size_t Geocoder::Context::GetNumTokens() const { return m_tokens.size(); }

size_t Geocoder::Context::GetNumUsedTokens() const
{
  CHECK_LESS_OR_EQUAL(m_numUsedTokens, m_tokens.size(), ());
  return m_numUsedTokens;
}

strings::UniString const & Geocoder::Context::GetToken(size_t id) const
{
  CHECK_LESS(id, m_tokens.size(), ());
  return m_tokens[id];
}

void Geocoder::Context::MarkToken(size_t id, Type type)
{
  CHECK_LESS(id, m_tokens.size(), ());
  bool wasUsed = m_tokenTypes[id] != Type::Count;
  m_tokenTypes[id] = type;
  bool nowUsed = m_tokenTypes[id] != Type::Count;

  if (wasUsed && !nowUsed)
    --m_numUsedTokens;
  if (!wasUsed && nowUsed)
    ++m_numUsedTokens;
}

bool Geocoder::Context::IsTokenUsed(size_t id) const
{
  CHECK_LESS(id, m_tokens.size(), ());
  return m_tokenTypes[id] != Type::Count;
}

bool Geocoder::Context::AllTokensUsed() const { return m_numUsedTokens == m_tokens.size(); }

void Geocoder::Context::AddResult(base::GeoObjectId const & osmId, double certainty)
{
  m_beam.Add(osmId, certainty);
}

void Geocoder::Context::FillResults(vector<Result> & results) const
{
  results.clear();
  results.reserve(m_beam.GetEntries().size());

  set<decltype(m_beam)::Key> seen;
  for (auto const & e : m_beam.GetEntries())
  {
    if (!seen.insert(e.m_key).second)
      continue;

    results.emplace_back(e.m_key /* osmId */, e.m_value /* certainty */);
  }

  if (!results.empty())
  {
    auto const by = results.front().m_certainty;
    for (auto & r : results)
      r.m_certainty /= by;
  }

  ASSERT(is_sorted(results.rbegin(), results.rend(), base::LessBy(&Result::m_certainty)), ());
  ASSERT_LESS_OR_EQUAL(results.size(), kMaxResults, ());
}

vector<Geocoder::Layer> & Geocoder::Context::GetLayers() { return m_layers; }

vector<Geocoder::Layer> const & Geocoder::Context::GetLayers() const { return m_layers; }

// Geocoder ----------------------------------------------------------------------------------------
Geocoder::Geocoder(string const & pathToJsonHierarchy) : m_hierarchy(pathToJsonHierarchy) {}

void Geocoder::ProcessQuery(string const & query, vector<Result> & results) const
{
#if defined(DEBUG)
  base::Timer timer;
  SCOPE_GUARD(printDuration, [&timer]() {
    LOG(LINFO, ("Total geocoding time:", timer.ElapsedSeconds(), "seconds"));
  });
#endif

  Context ctx(query);
  Go(ctx, Type::Country);
  ctx.FillResults(results);
}

Hierarchy const & Geocoder::GetHierarchy() const { return m_hierarchy; }

void Geocoder::Go(Context & ctx, Type type) const
{
  if (ctx.GetNumTokens() == 0)
    return;

  if (ctx.AllTokensUsed())
    return;

  if (type == Type::Count)
    return;

  vector<strings::UniString> subquery;
  for (size_t i = 0; i < ctx.GetNumTokens(); ++i)
  {
    subquery.clear();
    for (size_t j = i; j < ctx.GetNumTokens(); ++j)
    {
      if (ctx.IsTokenUsed(j))
        break;

      subquery.push_back(ctx.GetToken(j));

      Layer curLayer;
      curLayer.m_type = type;

      // Buildings are indexed separately.
      if (type == Type::Building)
      {
        FillBuildingsLayer(ctx, subquery, curLayer);
      }
      else
      {
        FillRegularLayer(ctx, type, subquery, curLayer);
      }

      if (curLayer.m_entries.empty())
        continue;

      ScopedMarkTokens mark(ctx, type, i, j + 1);

      // double const certainty =
      //     static_cast<double>(ctx.GetNumUsedTokens()) /
      //     static_cast<double>(ctx.GetNumTokens());

      double certainty = 0;
      for (auto const t : ctx.GetTokenTypes())
      {
        auto const it = kWeight.find(t);
        CHECK(it != kWeight.end(), ());
        certainty += it->second;
      }

      for (auto const * e : curLayer.m_entries)
      {
        ctx.AddResult(e->m_osmId, certainty);
      }

      ctx.GetLayers().emplace_back(move(curLayer));
      SCOPE_GUARD(pop, [&] { ctx.GetLayers().pop_back(); });

      Go(ctx, NextType(type));
    }
  }

  Go(ctx, NextType(type));
}

void Geocoder::FillBuildingsLayer(Context const & ctx, Tokens const & subquery,
                                  Layer & curLayer) const
{
  if (ctx.GetLayers().empty())
    return;
  auto const & layer = ctx.GetLayers().back();
  if (layer.m_type != Type::Street)
    return;

  auto const & subqueryHN = MakeHouseNumber(subquery);

  if (!search::house_numbers::LooksLikeHouseNumber(subqueryHN, false /* isPrefix */))
    return;

  for (auto const & se : layer.m_entries)
  {
    for (auto const & be : se->m_buildingsOnStreet)
    {
      auto const bt = static_cast<size_t>(Type::Building);
      auto const & realHN = MakeHouseNumber(be->m_address[bt]);
      if (search::house_numbers::HouseNumbersMatch(realHN, subqueryHN, false /* queryIsPrefix */))
        curLayer.m_entries.emplace_back(be);
    }
  }
}

void Geocoder::FillRegularLayer(Context const & ctx, Type type, Tokens const & subquery,
                                Layer & curLayer) const
{
  auto const * entries = m_hierarchy.GetEntries(subquery);
  if (!entries || entries->empty())
    return;

  for (auto const * e : *entries)
  {
    if (e->m_type != type)
      continue;

    if (ctx.GetLayers().empty() || FindParent(ctx.GetLayers(), *e))
      curLayer.m_entries.emplace_back(e);
  }
}
}  // namespace geocoder
