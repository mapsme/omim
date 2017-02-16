#include "jni_helper.hpp"
#include "logging.hpp"
#include "base/exception.hpp"
#include "base/logging.hpp"
#include "coding/file_writer.hpp"
#include "platform/platform.hpp"
#include "../util/crashlytics.h"

#include <android/log.h>
#include <cassert>
#include <cstdlib>


extern crashlytics_context_t * g_crashlytics;

namespace jni
{

using namespace my;

void AndroidMessage(LogLevel level, SrcPoint const & src, string const & s)
{
  android_LogPriority pr = ANDROID_LOG_SILENT;

  switch (level)
  {
    case LINFO: pr = ANDROID_LOG_INFO; break;
    case LDEBUG: pr = ANDROID_LOG_DEBUG; break;
    case LWARNING: pr = ANDROID_LOG_WARN; break;
    case LERROR: pr = ANDROID_LOG_ERROR; break;
    case LCRITICAL: pr = ANDROID_LOG_ERROR; break;
  }

  JNIEnv * env = jni::GetEnv();
  static jmethodID const logCoreMsgMethod = jni::GetStaticMethodID(env, g_loggerFactory,
     "logCoreMessage", "(ILjava/lang/String;)V");

  string const out = DebugPrint(src) + " " + s;
  env->CallStaticVoidMethod(g_loggerFactory, logCoreMsgMethod, pr, jni::ToJavaString(env, out));
  if (g_crashlytics)
    g_crashlytics->log(g_crashlytics, out.c_str());
}

void AndroidLogMessage(LogLevel level, SrcPoint const & src, string const & s)
{
  AndroidMessage(level, src, s);
  CHECK_LESS(level, g_LogAbortLevel, ("Abort. Log level is too serious", level));
}

void AndroidAssertMessage(SrcPoint const & src, string const & s)
{
  AndroidMessage(LCRITICAL, src, s);
#ifdef DEBUG
  assert(false);
#else
  std::abort();
#endif
}

void InitSystemLog()
{
  SetLogMessageFn(&AndroidLogMessage);
}

void InitAssertLog()
{
  SetAssertFunction(&AndroidAssertMessage);
}

void ToggleDebugLogs(bool enabled)
{
  if (enabled)
    g_LogLevel = LDEBUG;
  else
    g_LogLevel = LINFO;
}
}

extern "C" {

void DbgPrintC(char const * format, ...)
{
  va_list argptr;
  va_start(argptr, format);

  __android_log_vprint(ANDROID_LOG_INFO, "MapsWithMe_Debug", format, argptr);

  va_end(argptr);
}

}
