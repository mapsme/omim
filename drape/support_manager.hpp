#pragma once

#include <boost/noncopyable.hpp>

namespace dp
{

class SupportManager : public boost::noncopyable
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

private:
  SupportManager() = default;
  ~SupportManager() = default;

  bool m_isSamsungGoogleNexus = false;
  bool m_isAdreno200 = false;
  bool m_isTegra = false;

  int m_maxLineWidth = 1;
};

} // namespace dp
