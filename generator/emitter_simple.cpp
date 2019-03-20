#include "generator/emitter_simple.hpp"

#include "generator/feature_builder.hpp"

namespace generator
{
EmitterSimple::EmitterSimple(feature::GenerateInfo const & info) :
  m_regionGenerator(std::make_unique<SimpleGenerator>(info)) {}

void EmitterSimple::GetNames(std::vector<std::string> & names) const
{
  if (m_regionGenerator)
    names = m_regionGenerator->Parent().Names();
  else
    names.clear();
}

void EmitterSimple::operator()(FeatureBuilder1 & fb)
{
  auto & emitter = m_regionGenerator->Parent();

  emitter.Start();
  (*m_regionGenerator)(fb);
  emitter.Finish();
}
}  // namespace generator
