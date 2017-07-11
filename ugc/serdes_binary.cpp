#include "ugc/serdes_binary.hpp"

#include <set>

using namespace std;

namespace ugc
{
namespace
{
class BaseCollector
{
public:
  virtual void VisitRating(float const f, char const * /* name */ = nullptr) {}
  virtual void operator()(string const & /* s */, char const * /* name */ = nullptr) {}
  virtual void operator()(Sentiment const /* sentiment */, char const * /* name */ = nullptr) {}
  virtual void operator()(Time const /* time */, char const * /* name */ = nullptr) {}
  virtual void operator()(TranslationKey const & tk, char const * /* name */ = nullptr) {}
  virtual void operator()(Text const & text, char const * /* name */ = nullptr) {}

  template <typename T>
  void operator()(vector<T> const & vs, char const * /* name */ = nullptr)
  {
    for (auto const & v : vs)
      (*this)(v);
  }

  template <typename R>
  typename enable_if<is_fundamental<R>::value>::type operator()(R const & r,
                                                                char const * /* name */ = nullptr)
  {
  }

  template <typename R>
  typename enable_if<!is_fundamental<R>::value>::type operator()(R const & r,
                                                                 char const * /* name */ = nullptr)
  {
    r.Visit(*this);
  }
};

// Collects all translation keys from UGC.
class TranslationKeyCollector : public BaseCollector
{
public:
  TranslationKeyCollector(set<TranslationKey> & keys) : m_keys(keys) {}

  void operator()(TranslationKey const & tk, char const * /* name */ = nullptr) override
  {
    m_keys.insert(tk.m_key);
  }

  template <typename T>
  void operator()(T const & t, char const * name = nullptr)
  {
    BaseCollector::operator()(t, name);
  }

private:
  set<TranslationKey> & m_keys;
};

// Collects all texts from UGC.
class TextCollector : public BaseCollector
{
public:
  TextCollector(vector<Text> & texts) : m_texts(texts) {}

  void operator()(Text const & text, char const * /* name */ = nullptr) override
  {
    m_texts.push_back(text);
  }

  template <typename T>
  void operator()(T const & t, char const * name = nullptr)
  {
    BaseCollector::operator()(t, name);
  }

private:
  vector<Text> & m_texts;
};
}  // namespace

void UGCSeriaizer::CollectTranslationKeys()
{
  ASSERT(m_keys.empty(), ());

  set<TranslationKey> keys;
  TranslationKeyCollector collector(keys);
  for (auto const & p : m_ugcs)
    collector(p.m_ugc);
  m_keys.assign(keys.begin(), keys.end());
}

void UGCSeriaizer::CollectTexts()
{
  ASSERT(m_texts.empty(), ());
  for (auto const & p : m_ugcs)
  {
    m_texts.emplace_back();
    TextCollector collector(m_texts.back());
    collector(p.m_ugc);
  }
}
}  // namespace ugc
