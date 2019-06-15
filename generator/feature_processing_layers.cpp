#include "generator/feature_processing_layers.hpp"

#include "generator/city_boundary_processor.hpp"
#include "generator/coastlines_generator.hpp"
#include "generator/feature_builder.hpp"
#include "generator/feature_maker.hpp"
#include "generator/generate_info.hpp"
#include "generator/type_helper.hpp"

#include "indexer/classificator.hpp"
#include "indexer/feature_visibility.hpp"
#include "indexer/ftypes_matcher.hpp"

#include "base/assert.hpp"
#include "base/geo_object_id.hpp"

using namespace feature;

namespace generator
{
namespace
{
void FixLandType(FeatureBuilder & feature)
{
  auto const & types = feature.GetTypes();
  auto const & isIslandChecker = ftypes::IsIslandChecker::Instance();
  auto const & isLandChecker = ftypes::IsLandChecker::Instance();
  auto const & isCoastlineChecker = ftypes::IsCoastlineChecker::Instance();
  if (isCoastlineChecker(types))
  {
    feature.PopExactType(isLandChecker.GetLandType());
    feature.PopExactType(isCoastlineChecker.GetCoastlineType());
  }
  else if (isIslandChecker(types) && feature.IsArea())
  {
    feature.AddType(isLandChecker.GetLandType());
  }
}
}  // namespace

std::string LogBuffer::GetAsString() const
{
  return m_buffer.str();
}

std::shared_ptr<LayerBase> LayerBase::CloneRecursive() const
{
  auto temp = shared_from_this();
  std::shared_ptr<LayerBase> clone;
  if (temp)
  {
    clone = temp->Clone();
    temp = temp->m_next;
  }
  while (temp)
  {
    clone->Add(temp->Clone());
    temp = temp->m_next;
  }
  return clone;
}

{
  if (m_next)
    m_next->Handle(feature);
}

void LayerBase::Merge(std::shared_ptr<LayerBase> const & other)
{
  CHECK(other, ());

  m_logBuffer.AppendLine(other->GetAsString());
}

void LayerBase::MergeRecursive(std::shared_ptr<LayerBase> const & other)
{
  auto left = shared_from_this();
  auto right = other;
  while (left && right)
  {
    left->Merge(right);
    left = left->m_next;
    right = right->m_next;
  }
}

void LayerBase::SetNext(std::shared_ptr<LayerBase> next)
{
  m_next = next;
}

std::shared_ptr<LayerBase> LayerBase::Add(std::shared_ptr<LayerBase> next)
{
  if (m_next)
    m_next->Add(next);
  else
    m_next = next;

  return next;
}

std::string LayerBase::GetAsString() const
{
  return m_logBuffer.GetAsString();
}

std::string LayerBase::GetAsStringRecursive() const
{
  std::ostringstream m_buffer;
  auto temp = shared_from_this();
  while (temp)
  {
    m_buffer << temp->GetAsString();
    temp = temp->m_next;
  }

  return m_buffer.str();
}

RepresentationLayer::RepresentationLayer() {}

std::shared_ptr<LayerBase> RepresentationLayer::Clone() const
{
  return std::make_shared<RepresentationLayer>();
}

{
  auto const sourceType = feature.GetMostGenericOsmId().GetType();
  auto const geomType = feature.GetGeomType();
  // There is a copy of params here, if there is a reference here, then the params can be
  // implicitly changed at other layers.
  auto const params = feature.GetParams();
  switch (sourceType)
  {
  case base::GeoObjectId::Type::ObsoleteOsmNode:
    LayerBase::Handle(feature);
    break;
  case base::GeoObjectId::Type::ObsoleteOsmWay:
  {
    switch (geomType)
    {
    case feature::GeomType::Area:
    {
      HandleArea(feature, params);
      if (CanBeLine(params))
      {
        auto featureLine = MakeLineFromArea(feature);
        LayerBase::Handle(featureLine);
      }
      break;
    }
    case feature::GeomType::Line:
      LayerBase::Handle(feature);
      break;
    default:
      UNREACHABLE();
      break;
    }
    break;
  }
  case base::GeoObjectId::Type::ObsoleteOsmRelation:
  {
    switch (geomType)
    {
    case feature::GeomType::Area:
      HandleArea(feature, params);
      break;
    default:
      UNREACHABLE();
      break;
    }
    break;
  }
  default:
    UNREACHABLE();
    break;
  }
}

void RepresentationLayer::HandleArea(FeatureBuilder & feature, FeatureParams const & params)
{
  if (CanBeArea(params))
  {
    LayerBase::Handle(feature);
    feature.SetParams(params);
  }
  else if (CanBePoint(params))
  {
    auto featurePoint = MakePointFromArea(feature);
    LayerBase::Handle(featurePoint);
  }
}

// static
bool RepresentationLayer::CanBeArea(FeatureParams const & params)
{
  return feature::IsDrawableLike(params.m_types, feature::GeomType::Area);
}

// static
bool RepresentationLayer::CanBePoint(FeatureParams const & params)
{
  return feature::HasUsefulType(params.m_types, feature::GeomType::Point);
}

// static
bool RepresentationLayer::CanBeLine(FeatureParams const & params)
{
  return feature::HasUsefulType(params.m_types, feature::GeomType::Line);
}

std::shared_ptr<LayerBase> PrepareFeatureLayer::Clone() const
{
  return std::make_shared<PrepareFeatureLayer>();
}

{
  auto const type = feature.GetGeomType();
  auto & params = feature.GetParams();
  feature::RemoveUselessTypes(params.m_types, type);
  feature.PreSerializeAndRemoveUselessNamesForIntermediate();
  FixLandType(feature);
  if (feature::HasUsefulType(params.m_types, type))
    LayerBase::Handle(feature);
}

std::shared_ptr<LayerBase> RepresentationCoastlineLayer::Clone() const

PromoCatalogLayer::PromoCatalogLayer(std::string const & citiesFinename)
  : m_cities(promo::LoadCities(citiesFinename))
{
}

void PromoCatalogLayer::Handle(FeatureBuilder & feature)
{
  if (ftypes::IsCityTownOrVillage(feature.GetTypes()))
  {
    if (m_cities.find(feature.GetMostGenericOsmId()) == m_cities.cend())
      return;

    auto static const kPromoType = classif().GetTypeByPath({"sponsored", "promo_catalog"});
    FeatureParams & params = feature.GetParams();
    params.AddType(kPromoType);
  }
  LayerBase::Handle(feature);
}

std::shared_ptr<LayerBase> RepresentationCoastlineLayer::Clone() const
{
  return std::make_shared<RepresentationCoastlineLayer>();
}

void RepresentationCoastlineLayer::Handle(FeatureBuilder & feature)
{
  auto const sourceType = feature.GetMostGenericOsmId().GetType();
  auto const geomType = feature.GetGeomType();
  switch (sourceType)
  {
  case base::GeoObjectId::Type::ObsoleteOsmNode:
    break;
  case base::GeoObjectId::Type::ObsoleteOsmWay:
  {
    switch (geomType)
    {
    case feature::GeomType::Area:
      LayerBase::Handle(feature);
      break;
    case feature::GeomType::Line:
      LayerBase::Handle(feature);
      break;
    default:
      UNREACHABLE();
      break;
    }
    break;
  }
  case base::GeoObjectId::Type::ObsoleteOsmRelation:
    break;
  default:
    UNREACHABLE();
    break;
  }
}

std::shared_ptr<LayerBase> PrepareCoastlineFeatureLayer::Clone() const
{
  return std::make_shared<PrepareCoastlineFeatureLayer>();
}

{
  if (feature.IsArea())
  {
    auto & params = feature.GetParams();
    feature::RemoveUselessTypes(params.m_types, feature.GetGeomType());
  }

  feature.PreSerializeAndRemoveUselessNamesForIntermediate();
  auto const & isCoastlineChecker = ftypes::IsCoastlineChecker::Instance();
  auto const kCoastType = isCoastlineChecker.GetCoastlineType();
  feature.SetType(kCoastType);
  LayerBase::Handle(feature);
}


std::shared_ptr<LayerBase> PreserializeLayer::Clone() const
{
  return std::make_shared<PreserializeLayer>();
}

void PreserializeLayer::Handle(FeatureBuilder1 & feature)
{
  if (feature.PreSerialize())
    LayerBase::Handle(feature);
}

AffilationsFeatureLayer::AffilationsFeatureLayer(std::shared_ptr<FeatureProcessorQueue> const & queue,
                                                 std::shared_ptr<feature::AffiliationInterface> const & affilation)
  : m_queue(queue), m_affilation(affilation) {}

std::shared_ptr<LayerBase> AffilationsFeatureLayer::Clone() const
{
  return std::make_shared<AffilationsFeatureLayer>(m_queue, m_affilation);
}

void AffilationsFeatureLayer::Handle(FeatureBuilder1 & feature)
{
  m_queue->Push({{feature, m_affilation->GetAffiliations(feature)}});
}
}  // namespace generator
