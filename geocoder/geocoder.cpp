#include "geocoder/geocoder.hpp"

#include "indexer/search_string_utils.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"
#include "base/scope_guard.hpp"
#include "base/timer.hpp"

#include <algorithm>
#include <utility>

using namespace std;

namespace
{
// todo(@m) This is taken from search/geocoder.hpp. Refactor.
struct ScopedMarkTokens
{
  using Type = geocoder::Hierarchy::EntryType;

  // The range is [l, r).
  ScopedMarkTokens(geocoder::Geocoder::Context & context, Type const & type, size_t l, size_t r)
    : m_context(context), m_type(type), m_l(l), m_r(r)
  {
    ASSERT_LESS_OR_EQUAL(l, r, ());
    ASSERT_LESS_OR_EQUAL(r, context.GetNumTokens(), ());

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

geocoder::Hierarchy::EntryType NextType(geocoder::Hierarchy::EntryType const & type)
{
  CHECK_NOT_EQUAL(type, geocoder::Hierarchy::EntryType::Count, ());
  auto t = static_cast<size_t>(type);
  return static_cast<geocoder::Hierarchy::EntryType>(t + 1);
}

bool FindParent(vector<geocoder::Geocoder::Layer> const & layers,
                geocoder::Hierarchy::Entry const & e)
{
  for (auto const & layer : layers)
  {
    for (auto const * pe : layer.m_entries)
    {
      // Note that the relationship is somewhat inverted: every ancestor
      // is stored in the address but the nodes have no information
      // about their children.
      if (e.m_address[static_cast<size_t>(pe->m_type)] == pe->m_nameTokens)
        return true;
    }
  }
  return false;
}
}  // namespace

namespace geocoder
{
// Geocoder::Context -------------------------------------------------------------------------------
Geocoder::Context::Context(string const & query)
{
  search::NormalizeAndTokenizeString(query, m_tokens);
  m_tokenTypes.assign(m_tokens.size(), Hierarchy::EntryType::Count);
  m_numUsedTokens = 0;
}

vector<Hierarchy::EntryType> & Geocoder::Context::GetTokenTypes() { return m_tokenTypes; }

size_t Geocoder::Context::GetNumTokens() const { return m_tokens.size(); }

size_t Geocoder::Context::GetNumUsedTokens() const
{
  ASSERT_LESS_OR_EQUAL(m_numUsedTokens, m_tokens.size(), ());
  return m_numUsedTokens;
}

strings::UniString const & Geocoder::Context::GetToken(size_t id) const
{
  ASSERT_LESS(id, m_tokens.size(), ());
  return m_tokens[id];
}

void Geocoder::Context::MarkToken(size_t id, Hierarchy::EntryType const & type)
{
  ASSERT_LESS(id, m_tokens.size(), ());
  bool wasUsed = m_tokenTypes[id] != Hierarchy::EntryType::Count;
  m_tokenTypes[id] = type;
  bool nowUsed = m_tokenTypes[id] != Hierarchy::EntryType::Count;

  if (wasUsed && !nowUsed)
    --m_numUsedTokens;
  if (!wasUsed && nowUsed)
    ++m_numUsedTokens;
}

bool Geocoder::Context::IsTokenUsed(size_t id) const
{
  ASSERT_LESS(id, m_tokens.size(), ());
  return m_tokenTypes[id] != Hierarchy::EntryType::Count;
}

bool Geocoder::Context::AllTokensUsed() const { return m_numUsedTokens == m_tokens.size(); }

void Geocoder::Context::AddResult(osm::Id const & osmId, double certainty)
{
  m_results[osmId] = max(m_results[osmId], certainty);
}

void Geocoder::Context::FillResults(std::vector<Result> & results) const
{
  results.clear();
  results.reserve(m_results.size());
  for (auto const & e : m_results)
    results.emplace_back(e.first /* osmId */, e.second /* certainty */);
}

std::vector<Geocoder::Layer> & Geocoder::Context::GetLayers() { return m_layers; }

std::vector<Geocoder::Layer> const & Geocoder::Context::GetLayers() const { return m_layers; }

// Geocoder ----------------------------------------------------------------------------------------
Geocoder::Geocoder(string pathToJsonHierarchy) : m_hierarchy(pathToJsonHierarchy) {}

void Geocoder::ProcessQuery(string const & query, vector<Result> & results) const
{
#if defined(DEBUG)
  my::Timer timer;
  MY_SCOPE_GUARD(printDuration, [&timer]() {
    LOG(LINFO, ("Total geocoding time:", timer.ElapsedSeconds(), "seconds"));
  });
#endif

  Context ctx(query);
  Go(ctx, Hierarchy::EntryType::Country);
  ctx.FillResults(results);
}

Hierarchy const & Geocoder::GetHierarchy() const { return m_hierarchy; }

void Geocoder::Go(Context & ctx, Hierarchy::EntryType const & type) const
{
  if (ctx.GetNumTokens() == 0)
    return;

  if (ctx.AllTokensUsed())
    return;

  if (type == Hierarchy::EntryType::Count)
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

      auto const * entries = m_hierarchy.GetEntries(subquery);
      if (!entries || entries->empty())
        continue;

      Layer curLayer;
      curLayer.m_type = type;
      for (auto const & e : *entries)
      {
        if (e.m_type != type)
          continue;

        if (ctx.GetLayers().empty() || FindParent(ctx.GetLayers(), e))
          curLayer.m_entries.emplace_back(&e);
      }

      if (!curLayer.m_entries.empty())
      {
        ScopedMarkTokens mark(ctx, type, i, j + 1);

        double const certainty =
            static_cast<double>(ctx.GetNumUsedTokens()) / static_cast<double>(ctx.GetNumTokens());

        for (auto const * e : curLayer.m_entries)
          ctx.AddResult(e->m_osmId, certainty);

        ctx.GetLayers().emplace_back(move(curLayer));
        MY_SCOPE_GUARD(pop, [&] { ctx.GetLayers().pop_back(); });

        Go(ctx, NextType(type));
      }
    }
  }

  Go(ctx, NextType(type));
}
}  // namespace geocoder
