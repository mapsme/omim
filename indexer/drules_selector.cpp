#include "indexer/drules_selector.hpp"
#include "indexer/drules_selector_parser.hpp"
#include "indexer/ftypes_matcher.hpp"
#include "indexer/scales.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"

namespace drule
{

namespace
{

class CompositeSelector : public ISelector
{
public:
  explicit CompositeSelector(size_t capacity)
  {
    m_selectors.reserve(capacity);
  }

  void Add(unique_ptr<ISelector> && selector)
  {
    m_selectors.emplace_back(move(selector));
  }

  // ISelector overrides:
  bool Test(FeatureType const & ft) const override
  {
    for (auto const & selector : m_selectors)
      if (!selector->Test(ft))
        return false;
    return true;
  }

private:
  vector<unique_ptr<ISelector>> m_selectors;
};

// Runtime feature style selector implementation
template <typename TType>
class Selector : public ISelector
{
public:
  // Signature of function which takes a property from a feature
  typedef bool (*TGetFeatureTagValueFn)(FeatureType const &, TType & value);

  Selector(TGetFeatureTagValueFn fn, SelectorOperatorType op, TType const & value)
    : m_getFeatureValueFn(fn), m_evalFn(nullptr), m_value(value)
  {
    ASSERT(fn != nullptr, ());

    switch (op)
    {
    case SelectorOperatorUnknown: m_evalFn = nullptr; break;
    case SelectorOperatorNotEqual: m_evalFn = &Selector<TType>::NotEqual; break;
    case SelectorOperatorLessOrEqual: m_evalFn = &Selector<TType>::LessOrEqual; break;
    case SelectorOperatorGreaterOrEqual: m_evalFn = &Selector<TType>::GreaterOrEqual; break;
    case SelectorOperatorEqual: m_evalFn = &Selector<TType>::Equal; break;
    case SelectorOperatorLess: m_evalFn = &Selector<TType>::Less; break;
    case SelectorOperatorGreater: m_evalFn = &Selector<TType>::Greater; break;
    case SelectorOperatorIsNotSet: m_evalFn = &Selector<TType>::IsNotSet; break;
    case SelectorOperatorIsSet: m_evalFn = &Selector<TType>::IsSet; break;
    }

    ASSERT(m_evalFn != nullptr, ("Unknown or unexpected selector operator type"));
    if (nullptr == m_evalFn)
      m_evalFn = &Selector<TType>::Unknown;
  }

  // ISelector overrides:
  bool Test(FeatureType const & ft) const override
  {
    TType tagValue;
    if (!m_getFeatureValueFn(ft, tagValue))
      return false;
    return (this->*m_evalFn)(tagValue);
  }

private:
  bool Unknown(TType const &) const { return false; }
  bool NotEqual(TType const & tagValue) const { return tagValue != m_value; }
  bool LessOrEqual(TType const & tagValue) const { return tagValue <= m_value; }
  bool GreaterOrEqual(TType const & tagValue) const { return tagValue >= m_value; }
  bool Equal(TType const & tagValue) const { return tagValue == m_value; }
  bool Less(TType const & tagValue) const { return tagValue < m_value; }
  bool Greater(TType const & tagValue) const { return tagValue > m_value; }
  bool IsNotSet(TType const & tagValue) const { return tagValue == TType(); }
  bool IsSet(TType const & tagValue) const { return tagValue != TType(); }

  typedef bool (Selector<TType>::*TOperationFn)(TType const &) const;

  TGetFeatureTagValueFn m_getFeatureValueFn;
  TOperationFn m_evalFn;
  TType const m_value;
};

// Feature tag value evaluator for tag 'population'
bool GetPopulation(FeatureType const & ft, uint64_t & population)
{
  population = ftypes::GetPopulation(ft);
  return true;
}

// Feature tag value evaluator for tag 'name'
bool GetName(FeatureType const & ft, string & name)
{
  ft.GetReadableName(name);
  return true;
}

// Feature tag value evaluator for tag 'bbox_area' (bounding box area in sq.meters)
bool GetBoundingBoxArea(FeatureType const & ft, double & sqM)
{
  if (feature::GEOM_AREA != ft.GetFeatureType())
    return false;

  m2::RectD const rect = ft.GetLimitRect(scales::GetUpperScale());

  sqM = MercatorBounds::AreaOnEarth(rect.LeftTop(), rect.LeftBottom(), rect.RightBottom()) +
        MercatorBounds::AreaOnEarth(rect.LeftTop(), rect.RightTop(), rect.RightBottom());
  return true;
}

// Feature tag value evaluator for tag 'rating'
bool GetRating(FeatureType const & ft, double & rating)
{
  if (!ftypes::IsHotelChecker::Instance()(ft))
    return false;

  double constexpr kDefaultRating = 0.0;

  string ratingStr = ft.GetMetadata().Get(feature::Metadata::FMD_RATING);
  if (ratingStr.empty() || !strings::to_double(ratingStr, rating))
    rating = kDefaultRating;
  return true;
}

// Add new tag value evaluator here

}  // namespace

unique_ptr<ISelector> ParseSelector(string const & str)
{
  SelectorExpression e;
  if (!ParseSelector(str, e))
  {
    // bad string format
    LOG(LDEBUG, ("Invalid selector format:", str));
    return unique_ptr<ISelector>();
  }

  if (e.m_tag == "population")
  {
    uint64_t value = 0;
    if (!e.m_value.empty() && !strings::to_uint64(e.m_value, value))
    {
      // bad string format
      LOG(LDEBUG, ("Invalid selector:", str));
      return unique_ptr<ISelector>();
    }
    return make_unique<Selector<uint64_t>>(&GetPopulation, e.m_operator, value);
  }
  else if (e.m_tag == "name")
  {
    return make_unique<Selector<string>>(&GetName, e.m_operator, e.m_value);
  }
  else if (e.m_tag == "bbox_area")
  {
    double value = 0;
    if (!e.m_value.empty() && (!strings::to_double(e.m_value, value) || value < 0))
    {
      // bad string format
      LOG(LDEBUG, ("Invalid selector:", str));
      return unique_ptr<ISelector>();
    }
    return make_unique<Selector<double>>(&GetBoundingBoxArea, e.m_operator, value);
  }
  else if (e.m_tag == "rating")
  {
    double value = 0;
    if (!e.m_value.empty() && (!strings::to_double(e.m_value, value) || value < 0))
    {
      // bad string format
      LOG(LDEBUG, ("Invalid selector:", str));
      return unique_ptr<ISelector>();
    }
    return make_unique<Selector<double>>(&GetRating, e.m_operator, value);
  }

  // Add new tag here

  // unrecognized selector
  LOG(LDEBUG, ("Unrecognized selector:", str));
  return unique_ptr<ISelector>();
}

unique_ptr<ISelector> ParseSelector(vector<string> const & strs)
{
  unique_ptr<CompositeSelector> cs = make_unique<CompositeSelector>(strs.size());

  for (string const & str : strs)
  {
    unique_ptr<ISelector> s = ParseSelector(str);
    if (nullptr == s)
    {
      LOG(LDEBUG, ("Invalid composite selector:", str));
      return unique_ptr<ISelector>();
    }
    cs->Add(move(s));
  }

  return unique_ptr<ISelector>(cs.release());
}

}  // namespace drule
