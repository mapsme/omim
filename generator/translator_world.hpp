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
// The TranslatorWorld class implements translator for building the world.
class TranslatorWorld : public Translator
{
public:
  explicit TranslatorWorld(std::shared_ptr<EmitterInterface> const & emitter,
                           std::shared_ptr<cache::IntermediateDataReader> const & cache,
                           feature::GenerateInfo const & info);

  // TranslatorInterface overrides:
  void Preprocess(OsmElement & element) override;

private:
  TagAdmixer m_tagAdmixer;
  TagReplacer m_tagReplacer;
};

// The TranslatorWorldWithAds class implements translator for building the world with advertising.
class TranslatorWorldWithAds : public TranslatorWorld
{
public:
  explicit TranslatorWorldWithAds(std::shared_ptr<EmitterInterface> const & emitter,
                                  std::shared_ptr<cache::IntermediateDataReader> const & cache,
                                  feature::GenerateInfo const & info);

  // TranslatorInterface overrides:
  void Preprocess(OsmElement & element) override;
  bool Finish() override;

private:
  OsmTagMixer m_osmTagMixer;
};
}  // namespace generator
