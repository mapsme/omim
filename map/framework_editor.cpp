// Separate framework implementation part, devoted to OSM Editor.

#include "framework.hpp"

#include "indexer/osm_editor.hpp"

void Framework::DeleteFeature(FeatureID const & fid)
{
  osm::Editor::DeleteFeature(fid);
  // TODO(vng): This invalidate does not work, at least on desktops.
  // For feature to disappear, user should drag or zoom the screen.
  // Check out drape behavior.
  Invalidate(true);
}

void Framework::EditFeatureName(FeatureID const & fid, string const & newName)
{
  osm::Editor::EditFeatureName(fid, newName, m_model.GetIndex());
  // TODO(vng): This invalidate does not work, at least on desktops.
  // For feature to disappear, user should drag or zoom the screen.
  // Check out drape behavior.
  Invalidate(true);
}
