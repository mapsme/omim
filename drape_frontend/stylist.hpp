#pragma once

#include "indexer/feature_data.hpp"
#include "indexer/ftypes_matcher.hpp"
#include "indexer/drawing_rule_def.hpp"

#include "base/buffer_vector.hpp"

#include <functional>
#include <string>

class FeatureType;

namespace drule { class BaseRule; }

namespace df
{

class IsBuildingHasPartsChecker : public ftypes::BaseChecker
{
  IsBuildingHasPartsChecker();
public:
  static IsBuildingHasPartsChecker const & Instance();
};

class IsBuildingPartChecker : public ftypes::BaseChecker
{
  IsBuildingPartChecker();
public:
  static IsBuildingPartChecker const & Instance();
};

class IsHatchingTerritoryChecker : public ftypes::BaseChecker
{
  IsHatchingTerritoryChecker();
public:
  static IsHatchingTerritoryChecker const & Instance();
};

struct CaptionDescription
{
  void Init(FeatureType const & f,
            int8_t deviceLang,
            int const zoomLevel,
            feature::EGeomType const type,
            drule::text_type_t const mainTextType,
            bool const auxCaptionExists);

  std::string const & GetMainText() const;
  std::string const & GetAuxText() const;
  std::string const & GetRoadNumber() const;
  std::string GetPathName() const;
  bool IsNameExists() const;

private:
  /// Clear aux name on high zoom and clear long main name on low zoom.
  void ProcessZoomLevel(int const zoomLevel);
  /// Try to use house number as name of the object.
  void ProcessMainTextType(drule::text_type_t const & mainTextType);

  std::string m_mainText;
  std::string m_auxText;
  std::string m_roadNumber;
  std::string m_houseNumber;
};

class Stylist
{
public:
  Stylist();

  bool IsCoastLine() const;
  bool AreaStyleExists() const;
  bool LineStyleExists() const;
  bool PointStyleExists() const;

  CaptionDescription const & GetCaptionDescription() const;

  using TRuleWrapper = pair<drule::BaseRule const *, double>;
  using TRuleCallback = std::function<void (TRuleWrapper const &)>;
  void ForEachRule(TRuleCallback const & fn) const;

  bool IsEmpty() const;

private:
  friend bool InitStylist(FeatureType const & f,
                          int8_t deviceLang,
                          int const zoomLevel,
                          bool buildings3d,
                          Stylist & s);

  void RaiseCoastlineFlag();
  void RaiseAreaStyleFlag();
  void RaiseLineStyleFlag();
  void RaisePointStyleFlag();

  CaptionDescription & GetCaptionDescriptionImpl();

private:
  typedef buffer_vector<TRuleWrapper, 8> rules_t;
  rules_t m_rules;

  uint8_t m_state;
  CaptionDescription m_captionDescriptor;
};

bool InitStylist(FeatureType const & f,
                 int8_t deviceLang,
                 int const zoomLevel,
                 bool buildings3d,
                 Stylist & s);

double GetFeaturePriority(FeatureType const & f, int const zoomLevel);

} // namespace df
