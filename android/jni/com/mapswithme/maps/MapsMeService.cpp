#include "../core/jni_helper.hpp"

#include "../../../../../base/logging.hpp"

#include "../../../search/result.hpp"

#include "Framework.hpp"

// TODO
// Get rid of statics and globals and think about OOP principles here.

//
// MapsMeService Init/UnInit
//

namespace
{
  // TODO
  // Since Framework callbacks are called out of Java thread, JNI call FindClass will return null.
  // Therefore we cache classes on init and drop cache on exit
  jclass g_classBitmap = nullptr;
  jclass g_classBitmapConfig = nullptr;
  jclass g_classLookupItem = nullptr;
}

void MapsMeService_Init(JNIEnv * env)
{
  g_classBitmap = static_cast<jclass>(env->NewGlobalRef(env->FindClass("android/graphics/Bitmap")));
  g_classBitmapConfig = static_cast<jclass>(env->NewGlobalRef(env->FindClass("android/graphics/Bitmap$Config")));
  g_classLookupItem = static_cast<jclass>(env->NewGlobalRef(env->FindClass("com/mapswithme/maps/MapsMeService$LookupService$LookupItem")));
}

void MapsMeService_UnInit(JNIEnv * env)
{
  env->DeleteGlobalRef(g_classBitmap);
  env->DeleteGlobalRef(g_classBitmapConfig);
  env->DeleteGlobalRef(g_classLookupItem);

  g_classBitmap = nullptr;
  g_classBitmapConfig = nullptr;
  g_classLookupItem = nullptr;
}

//
// RenderService functionality
//

namespace
{
namespace RenderServiceImpl
{
  // Wrappers for Android Bitmap manipulations

  jintArray createIntArray(JNIEnv * env, const int * data, int size)
  {
    jintArray const int_array = env->NewIntArray(size);
    env->SetIntArrayRegion(int_array, 0, size, (jint *)data);
    return int_array;
  }

  jobject createBitmapConfig(JNIEnv * env, const char * config)
  {
    jclass const config_class = g_classBitmapConfig;
    jmethodID const midValueOf = env->GetStaticMethodID(config_class, "valueOf", "(Ljava/lang/String;)Landroid/graphics/Bitmap$Config;");
    jstring const config_name = jni::ToJavaString(env, config);
    jobject const bitmap_config = env->CallStaticObjectMethod(config_class, midValueOf, config_name);
    env->DeleteLocalRef(config_name);

    return bitmap_config;
  }

  jobject createBitmap(JNIEnv * env, const int * colors, int width, int height, jobject bitmap_config)
  {
    jintArray const int_array = createIntArray(env, colors, width * height);
    jclass const bitmap_class = g_classBitmap;
    jmethodID const midCreateBitmap = env->GetStaticMethodID(bitmap_class,
        "createBitmap", "([IIIIILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
    jobject const bitmap = env->CallStaticObjectMethod(bitmap_class, midCreateBitmap, int_array, 0, width, width, height, bitmap_config);
    env->DeleteLocalRef(int_array);

    return bitmap;
  }

  jobject createBitmap(JNIEnv * env, MapImage const & mi)
  {
    // TODO
    // Support not only 4 bytes per pixel
    // Support images with width which is not a ratio of 4
    if (mi.m_bpp != sizeof(int))
      return nullptr;
    if (mi.m_width % sizeof(int) != 0)
      return nullptr;
    if (mi.m_bytes.empty() || mi.m_bytes.size() != (mi.m_width * mi.m_height * mi.m_bpp))
      return nullptr;

    int const width = mi.m_width;
    int const height = mi.m_height;
    const int * const colors = reinterpret_cast<const int *>(&mi.m_bytes[0]);

    jobject const bitmap_config = createBitmapConfig(env, "ARGB_8888");
    jobject const bitmap = createBitmap(env, colors, width, height, bitmap_config);
    env->DeleteLocalRef(bitmap_config);

    return bitmap;
  }

  // Helper to fire callback method on a subscriber

  void fireOnRenderComplete(JNIEnv * env, shared_ptr<jobject> const & listener, jobject bmp)
  {
    LOG(LDEBUG, ("MapsMeService_RenderService_fireOnRenderComplete"));

    jmethodID const methodID = jni::GetJavaMethodID(env, *listener.get(), "onRenderComplete", "(Landroid/graphics/Bitmap;)V");

    env->CallVoidMethod(*listener.get(), methodID, bmp);
  }

  // TODO
  // Support more than listener
  std::shared_ptr<jobject> g_renderServicelistener;

  // Framework's callback about render map

  void renderAsyncCallback(MapImage const & mi)
  {
    LOG(LDEBUG, ("MapsMeService_RenderService_renderAsyncCallback"));

    std::shared_ptr<jobject> const listener = g_renderServicelistener;
    if (listener.get() == nullptr)
    {
      LOG(LDEBUG, ("MapsMeService_RenderService_renderAsyncCallback: there is no listener"));
      return;
    }

    JNIEnv * const env = jni::GetEnv();

    jobject const bmp = createBitmap(env, mi);
    if (nullptr == bmp)
    {
      LOG(LDEBUG, ("MapsMeService_RenderService_renderAsyncCallback: bitmap wasn't created"));
      return;
    }

    fireOnRenderComplete(env, listener, bmp);

    env->DeleteLocalRef(bmp);
  }

} // namespace RenderServiceImpl
} // namespace

// Java methods for RenderService

extern "C"
{
  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_MapsMeService_00024RenderService_addListener(JNIEnv * env, jobject thiz, jobject obj)
  {
    LOG(LDEBUG, ("MapsMeService_RenderService_addListener"));

    using namespace RenderServiceImpl;

    std::shared_ptr<jobject> listener = jni::make_global_ref(obj);

    g_renderServicelistener = std::move(listener);

    GlobalSetRenderAsyncCallback(renderAsyncCallback);
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_MapsMeService_00024RenderService_removeListener(JNIEnv * env, jobject thiz, jobject obj)
  {
    LOG(LDEBUG, ("MapsMeService_RenderService_removeListener"));

    using namespace RenderServiceImpl;

    g_renderServicelistener.reset();
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

    using namespace RenderServiceImpl;

    std::shared_ptr<jobject> const listener = g_renderServicelistener;
    if (nullptr == listener.get())
    {
      LOG(LDEBUG, ("MapsMeService_RenderService_renderMap: there is no listener"));
      return;
    }

    m2::PointD const origin = MercatorBounds::FromLatLon(originLatitude, originLongitude);
    GlobalRenderAsync(origin, static_cast<size_t>(scale), static_cast<size_t>(imageWidth), static_cast<size_t>(imageHeight));
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_MapsMeService_00024RenderService_allowRenderingInBackground(JNIEnv * env, jobject thiz,
                                                jboolean allow)
  {
    LOG(LDEBUG, ("MapsMeService_RenderService_allowRenderingInBackground: ", allow));

    SetAppInBackground(allow ? true : false);
  }

} // extern "C"

//
// LookupService functionality
//

namespace
{
namespace LookupServiceImpl
{
  // TODO
  // Support more than listener
  std::shared_ptr<jobject> g_lookupServicelistener;

  // JNI helpers

  jobject createLookupItem(JNIEnv * env)
  {
    jclass const klass = g_classLookupItem;

    jmethodID const methodID = env->GetMethodID(klass, "<init>", "(Lcom/mapswithme/maps/MapsMeService$LookupService;)V");
    jobject const obj = env->NewObject(klass, methodID, nullptr);

    return obj;
  }

  const int TYPE_SUGGESTION = 0; // keep it synchronous with MapsMeService.java
  const int TYPE_FEATURE = 1; // keep it synchronous with MapsMeService.java

  jobject createLookupItem(JNIEnv * env, search::Result const & result)
  {
    jclass const klass = g_classLookupItem;

    jobject const obj = createLookupItem(env);

    jfieldID const nameId = env->GetFieldID(klass, "mName", "Ljava/lang/String;");
    jstring str = jni::ToJavaString(env, result.GetString());
    env->SetObjectField(obj, nameId, str);
    env->DeleteLocalRef(str);

    jfieldID const regionId = env->GetFieldID(klass, "mRegion", "Ljava/lang/String;");
    str = jni::ToJavaString(env, result.GetRegionString());
    env->SetObjectField(obj, regionId, str);
    env->DeleteLocalRef(str);

    jfieldID const amenityId = env->GetFieldID(klass, "mAmenity", "Ljava/lang/String;");
    str = jni::ToJavaString(env, result.GetFeatureType());
    env->SetObjectField(obj, amenityId, str);
    env->DeleteLocalRef(str);

    if (result.IsSuggest())
    {
      jfieldID const suggestionId = env->GetFieldID(klass, "mSuggestion", "Ljava/lang/String;");
      str = jni::ToJavaString(env, result.GetSuggestionString());
      env->SetObjectField(obj, suggestionId, str);
      env->DeleteLocalRef(str);
    }

    jint const type = result.IsSuggest() ? TYPE_SUGGESTION : TYPE_FEATURE;
    jfieldID const typeId = env->GetFieldID(klass, "mType", "I");
    env->SetIntField(obj, typeId, type);

    jdouble lat = 0.0, lon = 0.0;
    if (result.HasPoint())
    {
      m2::PointD const pt = result.GetFeatureCenter();
      lat = MercatorBounds::YToLat(pt.y);
      lon = MercatorBounds::XToLon(pt.x);
    }

    jfieldID const latId = env->GetFieldID(klass, "mLatitude", "D");
    env->SetDoubleField(obj, latId, lat);

    jfieldID const lonId = env->GetFieldID(klass, "mLongitude", "D");
    env->SetDoubleField(obj, lonId, lon);

    return obj;
  }

  jobjectArray createLookupItemsArray(JNIEnv * env, search::Results const & results)
  {
    size_t const count = results.GetCount();

    jclass const klass = g_classLookupItem;

    jobjectArray const arr = (jobjectArray)env->NewObjectArray(count, klass, nullptr);
    if (nullptr == arr)
      return nullptr;

    auto i = results.Begin();
    auto const iend = results.End();
    for (size_t index = 0; i != iend; ++i, ++index)
    {
      jobject const item = createLookupItem(env, *i);
      if (nullptr == item)
      {
        env->DeleteLocalRef(arr);
        return nullptr;
      }
      env->SetObjectArrayElement(arr, index, item);
      env->DeleteLocalRef(item);
    }

    return arr;
  }

  // Helper to fire callback method on a subscriber

  void fireOnLookupComplete(JNIEnv * env, shared_ptr<jobject> const & listener, jobjectArray arr)
  {
    LOG(LDEBUG, ("MapsMeService_LookupService_fireOnLookupComplete"));

    jmethodID const methodID = jni::GetJavaMethodID(env, *listener.get(), "onLookupComplete",
        "([Lcom/mapswithme/maps/MapsMeService$LookupService$LookupItem;)V");

    env->CallVoidMethod(*listener.get(), methodID, arr);
  }

  // Lookup callback function

  void lookupCallback(search::Results const & results)
  {
    if (results.IsEndMarker())
    {
      LOG(LDEBUG, ("MapsMeService_LookupService_lookupCallback: end marker"));
      return;
    }

    size_t const count = results.GetCount();

    LOG(LDEBUG, ("MapsMeService_LookupService_lookupCallback: count=", count));

    if (0 == count)
      return;

    std::shared_ptr<jobject> const listener = g_lookupServicelistener;
    if (listener.get() == nullptr)
    {
      LOG(LDEBUG, ("MapsMeService_LookupService_lookupCallback: there is no listener"));
      return;
    }

    JNIEnv * const env = jni::GetEnv();

    jobjectArray const arr = createLookupItemsArray(env, results);
    if (nullptr == arr)
    {
      LOG(LDEBUG, ("MapsMeService_LookupService_lookupCallback: array wasn't created"));
      return;
    }

    fireOnLookupComplete(env, listener, arr);

    env->DeleteLocalRef(arr);
  }

} // namespace LookupServiceImpl
} // namespace

extern "C"
{
  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_MapsMeService_00024LookupService_addListener(JNIEnv * env, jobject thiz, jobject obj)
  {
    LOG(LDEBUG, ("MapsMeService_LookupService_addListener"));

    using namespace LookupServiceImpl;

    std::shared_ptr<jobject> listener = jni::make_global_ref(obj);

    g_lookupServicelistener = std::move(listener);
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_MapsMeService_00024LookupService_removeListener(JNIEnv * env, jobject thiz, jobject obj)
  {
    LOG(LDEBUG, ("MapsMeService_LookupService_removeListener"));

    using namespace LookupServiceImpl;

    g_lookupServicelistener.reset();
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_MapsMeService_00024LookupService_lookup(JNIEnv * env, jobject thiz,
                                              jdouble originLatitude,
                                              jdouble originLongitude,
                                              jint radiusMeters,
                                              jstring query,
                                              jstring language,
                                              jint count)
  {
    LOG(LDEBUG, ("MapsMeService_LookupService_lookup"));

    using namespace LookupServiceImpl;

    std::shared_ptr<jobject> const listener = g_lookupServicelistener;
    if (nullptr == listener.get())
    {
      LOG(LDEBUG, ("MapsMeService_LookupService_lookup: there is no listener"));
      return;
    }

    search::SearchParams params;
    params.m_callback = lookupCallback;
    params.SetForceSearch(true);
    params.SetPosition(originLatitude, originLongitude);
    params.SetSearchMode(search::SearchParams::AROUND_POSITION);

    if (nullptr != query)
    {
      params.m_query = jni::ToNativeString(env, query);
    }

    if (nullptr != language)
    {
      std::string str = jni::ToNativeString(env, language);
      params.SetInputLocale(str);
    }

    g_framework->Search(params);
  }

} // extern "C"
