#pragma once

#include "std/target_os.hpp"

#if defined(OMIM_OS_IPHONE)
  #include <OpenGLES/ES2/glext.h>
  #include <OpenGLES/ES3/gl.h>
#elif defined(OMIM_OS_MAC)
  #include <OpenGL/glext.h>
  #include <OpenGL/gl3.h>
#elif defined(OMIM_OS_WINDOWS)
  #include "std/windows.hpp"
  #define GL_GLEXT_PROTOTYPES
  #include <GL/gl.h>
  #include "3party/GL/glext.h"
#elif defined(OMIM_OS_ANDROID)
  #define GL_GLEXT_PROTOTYPES
  #include <GLES2/gl2.h>
  #include <GLES2/gl2ext.h>
  #include "android/jni/com/mapswithme/opengl/gl3stub.h"
  #include <EGL/egl.h>
#else
  #define GL_GLEXT_PROTOTYPES
  #include <GL/gl.h>
  #include <GL/glext.h>
#endif
