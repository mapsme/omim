#pragma once

#include "drape/gl_includes.hpp"

namespace my
{
class SrcPoint;
}  // namespace my

namespace android
{
class ConfigComparator
{
public:
  ConfigComparator(EGLDisplay display);

  bool operator()(EGLConfig const & l, EGLConfig const & r) const;
  int configWeight(EGLConfig const & config) const;
  int configAlphaSize(EGLConfig const & config) const;

private:
  EGLDisplay m_display;
};

void CheckEGL(my::SrcPoint const & src);
}  // namespace android

#define CHECK_EGL(x) do { (x); android::CheckEGL(SRC());} while(false);
#define CHECK_EGL_CALL() do { android::CheckEGL(SRC());} while (false);
