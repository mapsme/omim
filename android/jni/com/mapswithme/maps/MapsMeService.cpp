#include "../core/jni_helper.hpp"

#include "../../../../../base/logging.hpp"

#include "Framework.hpp"

static jclass g_classBitmap = nullptr;
static jclass g_classBitmapConfig = nullptr;

void MapsMeService_Init(JNIEnv * env)
{
  g_classBitmap = static_cast<jclass>(env->NewGlobalRef(env->FindClass("android/graphics/Bitmap")));
  g_classBitmapConfig = static_cast<jclass>(env->NewGlobalRef(env->FindClass("android/graphics/Bitmap$Config")));
}

void MapsMeService_UnInit(JNIEnv * env)
{
  env->DeleteGlobalRef(g_classBitmap);
  env->DeleteGlobalRef(g_classBitmapConfig);
  g_classBitmap = nullptr;
  g_classBitmapConfig = nullptr;
}

extern "C"
{
  static jobject createBitmapConfig(JNIEnv * env, const char * config)
  {
    LOG(LDEBUG, ("MapsMeService_RenderService_createBitmapConfig: config=", config));

    jclass const config_class = g_classBitmapConfig;
    jmethodID const midValueOf = env->GetStaticMethodID(config_class, "valueOf", "(Ljava/lang/String;)Landroid/graphics/Bitmap$Config;");
    jstring const config_name = jni::ToJavaString(env, config);
    jobject const bitmap_config = env->CallStaticObjectMethod(config_class, midValueOf, config_name);

    return bitmap_config;
  }

  static jobject createBitmap(JNIEnv * env, int width, int height, jobject bitmap_config)
  {
    LOG(LDEBUG, ("MapsMeService_RenderService_createBitmap: width=", width, ", height=", height));

    jclass const bitmap_class = g_classBitmap;
    jmethodID const midCreateBitmap = env->GetStaticMethodID(bitmap_class,
        "createBitmap", "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
    jobject const bitmap = env->CallStaticObjectMethod(bitmap_class, midCreateBitmap, width, height, bitmap_config);

    return bitmap;
  }

  static jintArray createIntArray(JNIEnv * env, int width, int height, const void * data)
  {
    LOG(LDEBUG, ("MapsMeService_RenderService_createIntArray: width=", width, ", height=", height));

    jintArray const int_array = env->NewIntArray(width * height);
    env->SetIntArrayRegion(int_array, 0, width * height, (jint *)data);

    return int_array;
  }

  static void bitmapSetPixels(JNIEnv * env, jobject bitmap, int width, int height, const void * data)
  {
    LOG(LDEBUG, ("MapsMeService_RenderService_bitmapSetPixels: width=", width, ", height=", height));

    jclass const bitmap_class = g_classBitmap;
    jmethodID const midSetPixels = env->GetMethodID(bitmap_class, "setPixels", "([IIIIIII)V");
    jintArray const int_array = createIntArray(env, width, height, data);
    env->CallVoidMethod(bitmap, midSetPixels, int_array, 0, width, 0, 0, width, height);
  }

  static jobject createBitmap(JNIEnv * env, int width, int height, int bpp, const void * data)
  {
    // TODO
    // Support not only 4 bytes per pixel
    // Support images with width which is not a ratio of 4
    if (bpp != sizeof(int))
    {
      LOG(LDEBUG, ("MapsMeService_RenderService_createBitmap: bpp is not 4"));
      return nullptr;
    }
    if (width % sizeof(int) != 0)
    {
      LOG(LDEBUG, ("MapsMeService_RenderService_createBitmap: width is not x4"));
      return nullptr;
    }

    jobject const bitmap_config = createBitmapConfig(env, "ARGB_8888");
    jobject const bitmap = createBitmap(env, width, height, bitmap_config);
    bitmapSetPixels(env, bitmap, width, height, data);

    return bitmap;
  }

  //
  //
  //

  static void fireOnRenderComplete(JNIEnv * env, shared_ptr<jobject> const & listener, jobject bmp)
  {
    LOG(LDEBUG, ("MapsMeService_RenderService_fireOnRenderComplete"));

    jmethodID const methodID = jni::GetJavaMethodID(env, *listener.get(), "onRenderComplete", "(Landroid/graphics/Bitmap;)V");

    env->CallVoidMethod(*listener.get(), methodID, bmp);
  }

  //
  //
  //

  static std::shared_ptr<jobject> g_listener;

  static volatile bool g_stopCallbacks = true;

  static void renderAsyncCallback(ExtraMapScreen::MapImage const & mi)
  {
    if (g_stopCallbacks)
    {
      LOG(LDEBUG, ("MapsMeService_RenderService_renderAsyncCallback: skipped tile image"));
      return;
    }

    LOG(LDEBUG, ("MapsMeService_RenderService_renderAsyncCallback"));

    std::shared_ptr<jobject> const listener = g_listener;
    if (listener.get() == nullptr)
    {
      LOG(LDEBUG, ("MapsMeService_RenderService_renderAsyncCallback: there is no listener"));
      return;
    }

    if (mi.m_bytes.empty())
    {
      LOG(LDEBUG, ("MapsMeService_RenderService_renderAsyncCallback: image is empty"));
      return;
    }

    JNIEnv * const env = jni::GetEnv();

    jobject const bmp = createBitmap(env, mi.m_width, mi.m_height, mi.m_bpp, &mi.m_bytes[0]);
    if (nullptr == bmp)
    {
      LOG(LDEBUG, ("MapsMeService_RenderService_renderAsyncCallback: bitmap wasn't created"));
      return;
    }

    g_stopCallbacks = true;

    fireOnRenderComplete(env, listener, bmp);
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_MapsMeService_00024RenderService_addListener(JNIEnv * env, jobject thiz, jobject obj)
  {
    LOG(LDEBUG, ("MapsMeService_RenderService_addListener"));

    std::shared_ptr<jobject> listener = jni::make_global_ref(obj);

    g_listener = std::move(listener);

    GlobalSetRenderAsyncCallback(renderAsyncCallback);
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_MapsMeService_00024RenderService_removeListener(JNIEnv * env, jobject thiz, jobject obj)
  {
    LOG(LDEBUG, ("MapsMeService_RenderService_removeListener"));

    g_listener.reset();
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_MapsMeService_00024RenderService_renderMap(JNIEnv * env, jobject thiz,
                                              jdouble originLatitude,
                                              jdouble originLongitude,
                                              jint scale,
                                              jint imageWidth,
                                              jint imageHeight,
                                              jdouble rotationAngleDegrees)
  {
    LOG(LDEBUG, ("MapsMeService_RenderService_renderMap"));

    std::shared_ptr<jobject> const listener = g_listener;
    if (nullptr == listener.get())
    {
      LOG(LDEBUG, ("MapsMeService_RenderService_renderMap: there is no listener"));
      return;
    }

    g_stopCallbacks = false;

    m2::PointD origin(originLongitude, originLatitude);
    GlobalRenderAsync(origin, static_cast<size_t>(scale), static_cast<size_t>(imageWidth), static_cast<size_t>(imageHeight));
  }
}
