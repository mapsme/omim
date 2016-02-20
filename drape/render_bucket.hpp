#pragma once

#include "drape/feature_geometry_decl.hpp"
#include "drape/pointers.hpp"

#include "std/function.hpp"
#include "std/limits.hpp"

class ScreenBase;

namespace df
{
class BatchMergeHelper;
}

namespace dp
{

class OverlayHandle;
class OverlayTree;
class VertexArrayBuffer;

class RenderBucket
{
  friend class df::BatchMergeHelper;
public:
  RenderBucket(drape_ptr<VertexArrayBuffer> && buffer);
  ~RenderBucket();

  ref_ptr<VertexArrayBuffer> GetBuffer();
  drape_ptr<VertexArrayBuffer> && MoveBuffer();

  size_t GetOverlayHandlesCount() const;
  drape_ptr<OverlayHandle> PopOverlayHandle();
  ref_ptr<OverlayHandle> GetOverlayHandle(size_t index);
  void AddOverlayHandle(drape_ptr<OverlayHandle> && handle);

  void Update(ScreenBase const & modelView);
  void CollectOverlayHandles(ref_ptr<OverlayTree> tree);
  void RemoveOverlayHandles(ref_ptr<OverlayTree> tree);
  void Render();

  // Only for testing! Don't use this function in production code!
  void RenderDebug(ScreenBase const & screen) const;

  // Only for testing! Don't use this function in production code!
  template <typename ToDo>
  void ForEachOverlay(ToDo const & todo)
  {
    for (drape_ptr<OverlayHandle> const & h : m_overlay)
      todo(make_ref(h));
  }

  bool IsShared() const { return !m_featuresGeometryInfo.empty(); }
  void StartFeatureRecord(FeatureGeometryId feature, m2::RectD const & limitRect);
  void EndFeatureRecord(bool featureCompleted);

  void SetFeatureMinZoom(int minZoom);
  int GetMinZoom() const { return m_featuresMinZoom; }

  using TCheckFeaturesWaiting = function<bool(m2::RectD const &)>;
  bool IsFeaturesWaiting(TCheckFeaturesWaiting isFeaturesWaiting);

  void AddFeaturesInfo(RenderBucket const & bucket);

private:
  struct FeatureGeometryInfo
  {
    FeatureGeometryInfo() = default;
    FeatureGeometryInfo(m2::RectD const & limitRect)
      : m_limitRect(limitRect)
    {}

    m2::RectD m_limitRect;
    bool m_featureCompleted = false;
  };
  using TFeaturesGeometryInfo = map<FeatureGeometryId, FeatureGeometryInfo>;
  using TFeatureInfo = pair<FeatureGeometryId, FeatureGeometryInfo>;

  int m_featuresMinZoom = numeric_limits<int>::max();
  TFeatureInfo m_featureInfo;
  TFeaturesGeometryInfo m_featuresGeometryInfo;

  vector<drape_ptr<OverlayHandle> > m_overlay;
  drape_ptr<VertexArrayBuffer> m_buffer;
};

} // namespace dp
