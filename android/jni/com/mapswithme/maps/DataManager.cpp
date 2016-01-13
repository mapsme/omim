#include "Framework.hpp"

#include "../core/jni_helper.hpp"

#include "platform/mwm_version.hpp"
#include "storage/storage.hpp"

#include "std/bind.hpp"
#include "std/shared_ptr.hpp"

namespace data
{

using namespace storage;

enum ItemCategory : uint32_t
{
  NEAR_ME,
  DOWNLOADED,
  OTHER,
};

enum ItemStatus : uint32_t
{
  UPDATABLE,
  DOWNLOADABLE,
  ENQUEUED,
  DONE,
  PROGRESS,
  FAILED,
};

jclass g_listClass;
jmethodID g_listAddMethod;
jclass g_countryItemClass;

Storage & GetStorage()
{
  return g_framework->Storage();
}

void PrepareClassRefs(JNIEnv * env)
{
  if (g_listClass)
    return;

  g_listClass = static_cast<jclass>(env->NewGlobalRef(env->FindClass("java/util/List")));
  g_listAddMethod = env->GetMethodID(g_listClass, "add", "(Ljava/lang/Object;)Z");
  g_countryItemClass = static_cast<jclass>(env->NewGlobalRef(env->FindClass("com/mapswithme/maps/downloader/CountryItem")));
}

string GetLocalizedName(string const & id)
{
  // TODO
  return id;
}

} // namespace data

extern "C"
{

using namespace storage;
using namespace data;

// Determines whether the legacy (large MWMs) mode is used.
//   static native boolean nativeIsLegacyMode();
JNIEXPORT jboolean JNICALL
Java_com_mapswithme_maps_downloader_DataManager_nativeIsLegacyMode(JNIEnv * env, jclass clazz)
{
  // TODO (trashkalmar): use appropriate method
  return version::IsSingleMwm(g_framework->Storage().GetCurrentDataVersion());
}

// Returns info about updatable data. Returns NULL on error.
//   static @Nullable UpdateInfo nativeGetUpdateInfo();
JNIEXPORT jobject JNICALL
Java_com_mapswithme_maps_downloader_DataManager_nativeGetUpdateInfo(JNIEnv * env, jclass clazz)
{
  // FIXME (trashkalmar): Uncomment after Storage::GetUpdateInfo() is implemented
  static Storage::UpdateInfo info = { 0 };
  //if (!GetStorage().GetUpdateInfo(info))
  //  return nullptr;

  static jclass const infoClass = static_cast<jclass>(env->NewGlobalRef(env->FindClass("com/mapswithme/maps/downloader/UpdateInfo")));
  ASSERT(infoClass, (jni::DescribeException()));
  static jmethodID const ctor = env->GetMethodID(infoClass, "<init>", "(II)V");
  ASSERT(ctor, (jni::DescribeException()));

  return env->NewObject(infoClass, ctor, info.m_numberOfMwmFilesToUpdate, info.m_totalUpdateSizeInBytes);
}

static void PutItemsToList(JNIEnv * env, jobject const list,  vector<TCountryId> const & children, TCountryId const & parent, function<void (jobject const &)> const & callback)
{
  static jmethodID const countryItemCtor = env->GetMethodID(g_countryItemClass, "<init>", "()V");
  static jfieldID const countryItemFieldId = env->GetFieldID(g_countryItemClass, "id", "Ljava/lang/String;");
  static jfieldID const countryItemFieldParentId = env->GetFieldID(g_countryItemClass, "parentId", "Ljava/lang/String;");
  static jfieldID const countryItemFieldName = env->GetFieldID(g_countryItemClass, "name", "Ljava/lang/String;");
  static jfieldID const countryItemFieldParentName = env->GetFieldID(g_countryItemClass, "parentName", "Ljava/lang/String;");

  jstring parentId = jni::ToJavaString(env, parent);
  jstring parentName = jni::ToJavaString(env, GetLocalizedName(parent));

  for (TCountryId const & child : children)
  {
    jobject item = env->NewObject(g_countryItemClass, countryItemCtor);

    // ID and parent`s ID
    env->SetObjectField(item, countryItemFieldId, jni::ToJavaString(env, child));
    env->SetObjectField(item, countryItemFieldParentId, parentId);

    // Localized name and parent`s name
    env->SetObjectField(item, countryItemFieldName, jni::ToJavaString(env, GetLocalizedName(child)));
    env->SetObjectField(item, countryItemFieldParentName, parentName);

    // Let the caller do special processing
    callback(item);

    // Put to resulting list
    env->CallBooleanMethod(list, g_listAddMethod, item);
    env->DeleteLocalRef(item);
  }
}

// Retrieves list of country items with its status info.
// Use root as parent if |parent| is null.
//   static void nativeListItems(@Nullable String parent, List<CountryItem> result);
JNIEXPORT void JNICALL
Java_com_mapswithme_maps_downloader_DataManager_nativeListItems(JNIEnv * env, jclass clazz, jstring parent, jobject result)
{
  PrepareClassRefs(env);

  Storage const & storage = GetStorage();
  TCountryId const parentId = (parent ? jni::ToNativeString(env, parent) : storage.GetRootId());

  if (parent)
  {
    vector<TCountryId> const children = storage.GetChildren(parentId);
    PutItemsToList(env, result, children, parentId, [](jobject const & item)
    {

    });
  }
  else
  {
    // TODO (trashkalmar): Countries near me

    // Downloaded
    vector<TCountryId> children;
    storage.GetDownloadedChildren(children);

    PutItemsToList(env, result, children, parentId, [](jobject const & item)
    {

    });

    //
  }

  //vector<TCountryId> const children = storage::GetChildren(parentId);
}

// Enqueue country in downloader.
//  static void nativeStartDownload(String countryId);
JNIEXPORT void JNICALL
Java_com_mapswithme_maps_downloader_DataManager_nativeStartDownload(JNIEnv * env, jclass clazz, jstring countryId)
{
  GetStorage().DownloadCountry(jni::ToNativeString(env, countryId), MapOptions::Map);
}

// Remove downloading country from downloader.
//  static void nativeCancelDownload(String countryId);
JNIEXPORT void JNICALL
Java_com_mapswithme_maps_downloader_DataManager_nativeCancelDownload(JNIEnv * env, jclass clazz, jstring countryId)
{
  GetStorage().DeleteFromDownloader(jni::ToNativeString(env, countryId));
}

static void StatusChangedCallback(shared_ptr<jobject> const & listenerRef, TCountryId const & countryId)
{
  JNIEnv * env = jni::GetEnv();

  jmethodID methodID = jni::GetJavaMethodID(env, *listenerRef.get(), "onStatusChanged", "(Ljava/lang/String;)V");
  env->CallVoidMethod(*listenerRef.get(), methodID, jni::ToJavaString(env, countryId));
}

static void ProgressChangedCallback(shared_ptr<jobject> const & listenerRef, TCountryId const & countryId, LocalAndRemoteSizeT const & sizes)
{
  JNIEnv * env = jni::GetEnv();

  jmethodID methodID = jni::GetJavaMethodID(env, *listenerRef.get(), "onProgress", "(Ljava/lang/String;JJ)V");
  env->CallVoidMethod(*listenerRef.get(), methodID, jni::ToJavaString(env, countryId), sizes.first, sizes.second);
}

// Registers callback about storage status changed.
// Returns slot ID which is should be used to unsubscribe.
//   static int nativeSubscribe(StorageCallback listener);
JNIEXPORT jint JNICALL
Java_com_mapswithme_maps_downloader_DataManager_nativeSubscribe(JNIEnv * env, jclass clazz, jobject listener)
{
  return GetStorage().Subscribe(bind(&StatusChangedCallback, jni::make_global_ref(listener), _1),
                                bind(&ProgressChangedCallback, jni::make_global_ref(listener), _1, _2));
}

// Unregisters storage status changed callback.
//  static void nativeUnsubscribe(int slot);
JNIEXPORT void JNICALL
Java_com_mapswithme_maps_downloader_DataManager_nativeUnsubscribe(JNIEnv * env, jclass clazz, jint slot)
{
  GetStorage().Unsubscribe(slot);
}

} // extern "C"
