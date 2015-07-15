#pragma once

#include "std/target_os.hpp"

#if defined(OMIM_OS_IPHONE)
  #include <OpenGLES/ES3/gl.h>
  #include <OpenGLES/ES3/glext.h>
#elif defined(OMIM_OS_MAC)
  #include <OpenGL/gl3.h>
  #include <OpenGL/glext.h>
#elif defined(OMIM_OS_WINDOWS)
  #include "std/windows.hpp"
  #define GL_GLEXT_PROTOTYPES
  #include <GL/gl.h>
  #include "3party/GL/glext.h"
#elif defined(OMIM_OS_ANDROID)
  #include <EGL/egl.h>
  #include "drape/android/gl3stub.h"
#else
  #define GL_GLEXT_PROTOTYPES
  #include <GL/gl.h>
  #include <GL/glext.h>
#endif
