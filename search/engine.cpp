#include "search/engine.hpp"

#include "search/geometry_utils.hpp"
#include "search/processor.hpp"
#include "search/search_params.hpp"

#include "storage/country_info_getter.hpp"

#include "indexer/categories_holder.hpp"
#include "indexer/classificator.hpp"
#include "indexer/scales.hpp"
#include "indexer/search_string_utils.hpp"

#include "platform/platform.hpp"

#include "geometry/distance_on_sphere.hpp"
#include "geometry/mercator.hpp"

#include "base/logging.hpp"
#include "base/scope_guard.hpp"
#include "base/stl_add.hpp"

#include "std/algorithm.hpp"
#include "std/bind.hpp"
#include "std/map.hpp"
#include "std/vector.hpp"

namespace search
{
namespace
{
class InitSuggestions
{
  using TSuggestMap = map<pair<strings::UniString, int8_t>, uint8_t>;
  TSuggestMap m_suggests;

public:
  void operator()(CategoriesHolder::Category::Name const & name)
  {
    if (name.m_prefixLengthToSuggest != CategoriesHolder::Category::kEmptyPrefixLength)
    {
      strings::UniString const uniName = NormalizeAndSimplifyString(name.m_name);

      uint8_t & score = m_suggests[make_pair(uniName, name.m_locale)];
      if (score == 0 || score > name.m_prefixLengthToSuggest)
        score = name.m_prefixLengthToSuggest;
    }
  }

  void GetSuggests(vector<Suggest> & suggests) const
  {
    suggests.reserve(suggests.size() + m_suggests.size());
    for (auto const & s : m_suggests)
      suggests.emplace_back(s.first.first, s.second, s.first.second);
  }
};
}  // namespace

// ProcessorHandle----------------------------------------------------------------------------------
ProcessorHandle::ProcessorHandle() : m_processor(nullptr), m_cancelled(false) {}

void ProcessorHandle::Cancel()
{
  lock_guard<mutex> lock(m_mu);
  m_cancelled = true;
  if (m_processor)
    m_processor->Cancel();
}

void ProcessorHandle::Attach(Processor & processor)
{
  lock_guard<mutex> lock(m_mu);
  m_processor = &processor;
  if (m_cancelled)
    m_processor->Cancel();
}

void ProcessorHandle::Detach()
{
  lock_guard<mutex> lock(m_mu);
  m_processor = nullptr;
}

// Engine::Params ----------------------------------------------------------------------------------
Engine::Params::Params() : m_locale("en"), m_numThreads(1) {}

Engine::Params::Params(string const & locale, size_t numThreads)
  : m_locale(locale), m_numThreads(numThreads)
{
}

// Engine ------------------------------------------------------------------------------------------
Engine::Engine(Index & index, CategoriesHolder const & categories,
               storage::CountryInfoGetter const & infoGetter, unique_ptr<ProcessorFactory> factory,
               Params const & params)
  : m_shutdown(false)
{
  InitSuggestions doInit;
  categories.ForEachName(bind<void>(ref(doInit), _1));
  doInit.GetSuggests(m_suggests);

  m_contexts.resize(params.m_numThreads);
  for (size_t i = 0; i < params.m_numThreads; ++i)
  {
    auto processor = factory->Build(index, categories, m_suggests, infoGetter);
    processor->SetPreferredLocale(params.m_locale);
    m_contexts[i].m_processor = move(processor);
  }

  m_threads.reserve(params.m_numThreads);
  for (size_t i = 0; i < params.m_numThreads; ++i)
    m_threads.emplace_back(&Engine::MainLoop, this, ref(m_contexts[i]));
}

Engine::~Engine()
{
  {
    lock_guard<mutex> lock(m_mu);
    m_shutdown = true;
    m_cv.notify_all();
  }

  for (auto & thread : m_threads)
    thread.join();
}

weak_ptr<ProcessorHandle> Engine::Search(SearchParams const & params, m2::RectD const & viewport)
{
  shared_ptr<ProcessorHandle> handle(new ProcessorHandle());
  PostMessage(Message::TYPE_TASK, [this, params, viewport, handle](Processor & processor)
              {
                DoSearch(params, viewport, handle, processor);
              });
  return handle;
}

void Engine::SetSupportOldFormat(bool support)
{
  PostMessage(Message::TYPE_BROADCAST, [this, support](Processor & processor)
              {
                processor.SupportOldFormat(support);
              });
}

void Engine::SetLocale(string const & locale)
{
  PostMessage(Message::TYPE_BROADCAST, [this, locale](Processor & processor)
              {
                processor.SetPreferredLocale(locale);
              });
}

void Engine::ClearCaches()
{
  PostMessage(Message::TYPE_BROADCAST, [this](Processor & processor)
              {
                processor.ClearCaches();
              });
}

void Engine::MainLoop(Context & context)
{
  while (true)
  {
    bool hasBroadcast = false;
    queue<Message> messages;

    {
      unique_lock<mutex> lock(m_mu);
      m_cv.wait(lock, [&]()
                {
                  return m_shutdown || !m_messages.empty() || !context.m_messages.empty();
                });

      if (m_shutdown)
        break;

      // As SearchEngine is thread-safe, there is a global order on
      // public API requests, and this order is kept by the global
      // |m_messages| queue.  When a broadcast message arrives, it
      // must be executed in any case by all threads, therefore the
      // first free thread extracts as many as possible broadcast
      // messages from |m_messages| front and replicates them to all
      // thread-specific |m_messages| queues.
      while (!m_messages.empty() && m_messages.front().m_type == Message::TYPE_BROADCAST)
      {
        for (auto & b : m_contexts)
          b.m_messages.push(m_messages.front());
        m_messages.pop();
        hasBroadcast = true;
      }

      // Consumes first non-broadcast message, if any.  We process
      // only a single task message (in constrast with broadcast
      // messages) because task messages are actually search queries,
      // whose processing may take an arbitrary amount of time. So
      // it's better to process only one message and leave rest to the
      // next free search thread.
      if (!m_messages.empty())
      {
        context.m_messages.push(move(m_messages.front()));
        m_messages.pop();
      }

      messages.swap(context.m_messages);
    }

    if (hasBroadcast)
      m_cv.notify_all();

    while (!messages.empty())
    {
      messages.front()(*context.m_processor);
      messages.pop();
    }
  }
}

template <typename... TArgs>
void Engine::PostMessage(TArgs &&... args)
{
  lock_guard<mutex> lock(m_mu);
  m_messages.emplace(forward<TArgs>(args)...);
  m_cv.notify_one();
}

void Engine::DoSearch(SearchParams const & params, m2::RectD const & viewport,
                      shared_ptr<ProcessorHandle> handle, Processor & processor)
{
  bool const viewportSearch = params.m_mode == Mode::Viewport;

  processor.Reset();
  processor.Init(viewportSearch);
  handle->Attach(processor);
  MY_SCOPE_GUARD(detach, [&handle]
                 {
                   handle->Detach();
                 });

  // Early exit when query processing is cancelled.
  if (processor.IsCancelled())
  {
    Results results;
    results.SetEndMarker(true /* isCancelled */);

    if (params.m_onResults)
      params.m_onResults(results);
    else
      LOG(LERROR, ("OnResults is not set."));
    return;
  }

  processor.Search(params, viewport);
}
}  // namespace search
