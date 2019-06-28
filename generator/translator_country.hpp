#pragma once

#include "generator/emitter_interface.hpp"
#include "generator/tag_admixer.hpp"
#include "generator/translator.hpp"

#include <memory>

namespace feature
{
struct GenerateInfo;
}  // namespace feature

namespace cache
{
class IntermediateDataReader;
}  // namespace cache

namespace generator
{
// The TranslatorArea class implements translator for building countries.
class TranslatorCountry : public Translator
{
public:
  explicit TranslatorCountry(std::shared_ptr<EmitterInterface> emitter,
                             cache::IntermediateDataReader & cache,
                             feature::GenerateInfo const & info);

  // TranslatorInterface overrides:
  void Preprocess(OsmElement & element) override;

private:
  void CollectFromRelations(OsmElement const & element);

  TagAdmixer m_tagAdmixer;
  TagReplacer m_tagReplacer;
};

// The TranslatorCountryWithAds class implements translator for building countries with advertising.
class TranslatorCountryWithAds : public TranslatorCountry
{
public:
  explicit TranslatorCountryWithAds(std::shared_ptr<EmitterInterface> emitter,
                                    cache::IntermediateDataReader & cache,
                                    feature::GenerateInfo const & info);

  // TranslatorInterface overrides:
  void Preprocess(OsmElement & element) override;
  bool Finish() override;

private:
  OsmTagMixer m_osmTagMixer;
};
}  // namespace generator
