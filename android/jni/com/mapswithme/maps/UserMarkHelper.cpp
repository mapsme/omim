#include "UserMarkHelper.hpp"

#include "map/place_page_info.hpp"

namespace usermark_helper
{
using search::AddressInfo;
using feature::Metadata;

void InjectMetadata(JNIEnv * env, jclass const clazz, jobject const mapObject, feature::Metadata const & metadata)
{
  static jmethodID const addId = env->GetMethodID(clazz, "addMetadata", "(ILjava/lang/String;)V");
  ASSERT(addId, ());

  for (auto const t : metadata.GetPresentTypes())
  {
    // TODO: It is not a good idea to pass raw strings to UI. Calling separate getters should be a better way.
    jni::TScopedLocalRef metaString(env, t == feature::Metadata::FMD_WIKIPEDIA ?
                                              jni::ToJavaString(env, metadata.GetWikiURL()) :
                                              jni::ToJavaString(env, metadata.Get(t)));
    env->CallVoidMethod(mapObject, addId, t, metaString.get());
  }
}

jobject CreateMapObject(JNIEnv * env, int mapObjectType, string const & title, string const & subtitle,
                        double lat, double lon, string const & address, Metadata const & metadata, string const & apiId)
{
  // public MapObject(@MapObjectType int mapObjectType, String title, String subtitle, double lat, double lon, String address, String apiId)
  static jmethodID const ctorId =
      jni::GetConstructorID(env, g_mapObjectClazz, "(ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;DDLjava/lang/String;)V");

  jobject mapObject = env->NewObject(g_mapObjectClazz, ctorId,
                                     mapObjectType,
                                     jni::ToJavaString(env, title),
                                     jni::ToJavaString(env, subtitle),
                                     jni::ToJavaString(env, address),
                                     lat, lon,
                                     jni::ToJavaString(env, apiId));

  InjectMetadata(env, g_mapObjectClazz, mapObject, metadata);
  return mapObject;
}

jobject CreateMapObject(JNIEnv * env, place_page::Info const & info)
{
  if (info.IsBookmark())
  {
    // public Bookmark(@IntRange(from = 0) int categoryId, @IntRange(from = 0) int bookmarkId, String name)
    static jmethodID const ctorId = jni::GetConstructorID(env, g_bookmarkClazz, "(IILjava/lang/String;)V");

    auto const & bac = info.GetBookmarkAndCategory();
    BookmarkCategory * cat = g_framework->NativeFramework()->GetBmCategory(bac.m_categoryIndex);
    BookmarkData const & data = static_cast<Bookmark const *>(cat->GetUserMark(bac.m_bookmarkIndex))->GetData();

    jni::TScopedLocalRef jName(env, jni::ToJavaString(env, data.GetName()));
    jobject mapObject = env->NewObject(g_bookmarkClazz, ctorId,
                                       static_cast<jint>(info.m_bac.m_categoryIndex),
                                       static_cast<jint>(info.m_bac.m_bookmarkIndex),
                                       jName.get());
    if (info.IsFeature())
      InjectMetadata(env, g_mapObjectClazz, mapObject, info.GetMetadata());
    return mapObject;
  }

  ms::LatLon const ll = info.GetLatLon();
  search::AddressInfo const address = g_framework->NativeFramework()->GetAddressInfoAtPoint(info.GetMercator());

  // TODO(yunikkk): object can be POI + API + search result + bookmark simultaneously.
  // TODO(yunikkk): Should we pass localized strings here and in other methods as byte arrays?
  if (info.IsMyPosition())
    return CreateMapObject(env, kMyPosition, info.GetTitle(), info.GetSubtitle(), ll.lat, ll.lon, address.FormatAddress(), {}, "");

  if (info.HasApiUrl())
    return CreateMapObject(env, kApiPoint, info.GetTitle(), info.GetSubtitle(), ll.lat, ll.lon, address.FormatAddress(), info.GetMetadata(), info.GetApiUrl());

  return CreateMapObject(env, kPoi, info.GetTitle(), info.GetSubtitle(), ll.lat, ll.lon, address.FormatAddress(),
                         info.IsFeature() ? info.GetMetadata() : Metadata(), "");
}
}  // namespace usermark_helper
