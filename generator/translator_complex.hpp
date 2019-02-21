#pragma once

#include "generator/collector_interface.hpp"
#include "generator/emitter_interface.hpp"
#include "generator/intermediate_data.hpp"
#include "generator/osm_element.hpp"
#include "generator/translator_geocoder_base.hpp"

#include <memory>

namespace generator
{
class TranslatorComplex : public TranslatorGeocoderBase
{
public:
    explicit TranslatorComplex(std::shared_ptr<EmitterInterface> emitter,
                               cache::IntermediateDataReader & holder)
        : TranslatorGeocoderBase(emitter, holder) {}

private:
    // TranslatorGeocoderBase overrides:
    bool IsSuitableElement(OsmElement const *) const override { return true; }
};
}  // namespace generator
