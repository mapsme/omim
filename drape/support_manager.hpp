#pragma once

#include "std/noncopyable.hpp"

#include <functional>

namespace dp
{

struct SupportedFeatures
{
  unsigned int m_buildings3D : 1;
  unsigned int m_perspectiveMode : 1;
};

using OnGetSupportedFeatures = std::function<void(SupportedFeatures const & features)>;

class SupportManager : public noncopyable
{
public:
  // This singleton must be available only from rendering threads.
  static SupportManager & Instance();

  // Initialization must be called only when OpenGL context is created.
  void Init();

  bool IsSamsungGoogleNexus() const;
  bool IsAdreno200Device() const;
  bool IsTegraDevice() const;

  int GetMaxLineWidth() const;

  SupportedFeatures const & GetFeatures() const;

private:
  SupportManager() = default;
  ~SupportManager() = default;

  bool m_isSamsungGoogleNexus = false;
  bool m_isAdreno200 = false;
  bool m_isTegra = false;

  int m_maxLineWidth = 1;

  SupportedFeatures m_features;
};

} // namespace dp
