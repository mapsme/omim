#pragma once

#include "base/assert.hpp"

#include <memory>
#include <string>
#include <vector>

struct OsmElement;

namespace generator
{
namespace cache
{
class IntermediateData;
}  // namespace cache

class TranslatorRegion;
class TranslatorGeoObjects;
class TranslatorCountry;
class TranslatorCountryWithAds;
class TranslatorCoastline;
class TranslatorWorld;
class TranslatorWorldWithAds;
class TranslatorStreets;
class TranslatorCollection;

// Implementing this interface allows an object to create intermediate data from OsmElement.
class TranslatorInterface
{
public:
  virtual ~TranslatorInterface() = default;

  virtual std::shared_ptr<TranslatorInterface>
  Clone(std::shared_ptr<cache::IntermediateData> const & = {}) const = 0;

  virtual void Preprocess(OsmElement &) {}
  virtual void Emit(OsmElement & element) = 0;
  virtual void Flush() = 0;
  virtual bool Finish() = 0;

  virtual void Merge(TranslatorInterface const *) = 0;

  virtual void MergeInto(TranslatorRegion *) const { FailIfMethodUnsuppirted(); }
  virtual void MergeInto(TranslatorGeoObjects *) const { FailIfMethodUnsuppirted(); }
  virtual void MergeInto(TranslatorCountry *) const { FailIfMethodUnsuppirted(); }
  virtual void MergeInto(TranslatorCountryWithAds *) const { FailIfMethodUnsuppirted(); }
  virtual void MergeInto(TranslatorCoastline *) const { FailIfMethodUnsuppirted(); }
  virtual void MergeInto(TranslatorWorld *) const { FailIfMethodUnsuppirted(); }
  virtual void MergeInto(TranslatorWorldWithAds *) const { FailIfMethodUnsuppirted(); }
  virtual void MergeInto(TranslatorStreets *) const { FailIfMethodUnsuppirted(); }
  virtual void MergeInto(TranslatorCollection *) const { FailIfMethodUnsuppirted(); }

private:
  void FailIfMethodUnsuppirted() const { CHECK(false, ("This method is unsupported.")); }
};
}  // namespace generator
