#pragma once

#include "std/string.hpp"

struct FeatureID;
class FeatureType;
class Framework;
class Index;
struct OSMEditorImpl;

/// Wrapper for all OSM edit operations.
class OSMEditor
{
  /// To hide heavy implementation and includes.
  OSMEditorImpl * m_impl;
  OSMEditor();

public:
  ~OSMEditor();

  /// @TODO(AlexZ): Framework is needed to invalidate. Index to read feature by id.
  /// Is there a better way to do it?
  /// @TODO(AlexZ): Framework's Invalidate does not work for edited features.
  void SetFrameworkAndIndex(Framework & framework, Index const & index);

  /// @returns true if original MWM feature was altered by editor.
  bool WasFeatureEdited(FeatureID const & featureId) const;
  /// @returns true if outFeature was initialized by altered feature from the editor.
  bool GetEditedFeature(FeatureID const & featureId, FeatureType & outFeature) const;

  /// Marks feature as "deleted" from MwM file.
  void DeleteFeature(FeatureID const & featureId);
  /// @TODO(AlexZ): What if name was edited for feature, where we do not draw names?
  void EditFeatureName(FeatureID const & featureId, string const & newName);

  static OSMEditor & Instance()
  {
    static OSMEditor instance;
    return instance;
  }
  void SaveToFile();
};
