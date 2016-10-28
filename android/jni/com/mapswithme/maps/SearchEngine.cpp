#include "Framework.hpp"

#include "search/everywhere_search_params.hpp"
#include "search/hotels_filter.hpp"
#include "search/mode.hpp"
#include "search/result.hpp"
#include "search/viewport_search_params.hpp"

#include "base/logging.hpp"

#include "std/cstdint.hpp"

#include "../core/jni_helper.hpp"
#include "../platform/Language.hpp"
#include "../platform/Platform.hpp"

using search::Results;
using search::Result;

namespace
{
class HotelsFilterBuilder
{
public:
  using Rule = shared_ptr<search::hotels_filter::Rule>;

  // *NOTE* keep this in sync with Java counterpart.
  enum Type
  {
    TYPE_AND = 0,
    TYPE_OR = 1,
    TYPE_OP = 2
  };

  // *NOTE* keep this in sync with Java counterpart.
  enum Field
  {
    FIELD_RATING = 0,
    FIELD_PRICE_RATE = 1
  };

  // *NOTE* keep this in sync with Java counterpart.
  enum Op
  {
    OP_LT = 0,
    OP_LE = 1,
    OP_GT = 2,
    OP_GE = 3,
    OP_EQ = 4
  };

  void Init(JNIEnv * env)
  {
    if (m_initialized)
      return;

    {
      auto const baseClass = env->FindClass("com/mapswithme/maps/search/HotelsFilter");
      m_type = env->GetFieldID(baseClass, "mType", "I");
    }

    {
      auto const andClass = env->FindClass("com/mapswithme/maps/search/HotelsFilter$And");
      m_andLhs = env->GetFieldID(andClass, "mLhs", "Lcom/mapswithme/maps/search/HotelsFilter;");
      m_andRhs = env->GetFieldID(andClass, "mRhs", "Lcom/mapswithme/maps/search/HotelsFilter;");
    }

    {
      auto const orClass = env->FindClass("com/mapswithme/maps/search/HotelsFilter$Or");
      m_orLhs = env->GetFieldID(orClass, "mLhs", "Lcom/mapswithme/maps/search/HotelsFilter;");
      m_orRhs = env->GetFieldID(orClass, "mRhs", "Lcom/mapswithme/maps/search/HotelsFilter;");
    }

    {
      auto const opClass = env->FindClass("com/mapswithme/maps/search/HotelsFilter$Op");
      m_field = env->GetFieldID(opClass, "mField", "I");
      m_op = env->GetFieldID(opClass, "mOp", "I");
    }

    {
      auto const ratingFilterClass =
          env->FindClass("com/mapswithme/maps/search/HotelsFilter$RatingFilter");
      m_rating = env->GetFieldID(ratingFilterClass, "mValue", "F");
    }

    {
      auto const priceRateFilterClass =
          env->FindClass("com/mapswithme/maps/search/HotelsFilter$PriceRateFilter");
      m_priceRate = env->GetFieldID(priceRateFilterClass, "mValue", "I");
    }

    m_initialized = true;
  }

  Rule Build(JNIEnv * env, jobject filter)
  {
    if (!m_initialized)
      return {};

    if (!filter)
      return {};

    auto const type = static_cast<int>(env->GetIntField(filter, m_type));

    switch (type)
    {
    case TYPE_AND: return BuildAnd(env, filter);
    case TYPE_OR: return BuildOr(env, filter);
    case TYPE_OP: return BuildOp(env, filter);
    }

    LOG(LERROR, ("Unknown type:", type));
    return {};
  }

private:
  Rule BuildAnd(JNIEnv * env, jobject filter)
  {
    auto const lhs = env->GetObjectField(filter, m_andLhs);
    auto const rhs = env->GetObjectField(filter, m_andRhs);
    return search::hotels_filter::And(Build(env, lhs), Build(env, rhs));
  }

  Rule BuildOr(JNIEnv * env, jobject filter)
  {
    auto const lhs = env->GetObjectField(filter, m_orLhs);
    auto const rhs = env->GetObjectField(filter, m_orRhs);
    return search::hotels_filter::Or(Build(env, lhs), Build(env, rhs));
  }

  Rule BuildOp(JNIEnv * env, jobject filter)
  {
    auto const field = static_cast<int>(env->GetIntField(filter, m_field));
    auto const op = static_cast<int>(env->GetIntField(filter, m_op));

    switch (field)
    {
    case FIELD_RATING: return BuildRatingOp(env, op, filter);
    case FIELD_PRICE_RATE: return BuildPriceRateOp(env, op, filter);
    }

    LOG(LERROR, ("Unknown field:", field));
    return {};
  }

  Rule BuildRatingOp(JNIEnv * env, int op, jobject filter)
  {
    using namespace search::hotels_filter;

    auto const rating = static_cast<float>(env->GetFloatField(filter, m_rating));

    switch (op)
    {
    case OP_LT: return Lt<Rating>(rating);
    case OP_LE: return Le<Rating>(rating);
    case OP_GT: return Gt<Rating>(rating);
    case OP_GE: return Ge<Rating>(rating);
    case OP_EQ: return Eq<Rating>(rating);
    }

    LOG(LERROR, ("Unknown op:", op));
    return {};
  }

  Rule BuildPriceRateOp(JNIEnv * env, int op, jobject filter)
  {
    using namespace search::hotels_filter;

    auto const priceRate = static_cast<int>(env->GetIntField(filter, m_priceRate));

    switch (op)
    {
    case OP_LT: return Lt<PriceRate>(priceRate);
    case OP_LE: return Le<PriceRate>(priceRate);
    case OP_GT: return Gt<PriceRate>(priceRate);
    case OP_GE: return Ge<PriceRate>(priceRate);
    case OP_EQ: return Eq<PriceRate>(priceRate);
    }

    LOG(LERROR, ("Unknown op:", op));
    return {};
  }

  jfieldID m_type;

  jfieldID m_andLhs;
  jfieldID m_andRhs;

  jfieldID m_orLhs;
  jfieldID m_orRhs;

  jfieldID m_field;
  jfieldID m_op;

  jfieldID m_rating;
  jfieldID m_priceRate;

  bool m_initialized = false;
} g_hotelsFilterBuilder;

// TODO yunitsky
// Do not cache search results here, after new search will be implemented.
// Currently we cannot serialize FeatureID of search result properly.
// Cache is needed to show results on the map after click in the list of results.
Results g_results;

// Timestamp of last search query. Results with older stamps are ignored.
uint64_t g_queryTimestamp;
// Implements 'NativeSearchListener' java interface.
jobject g_javaListener;
jmethodID g_updateResultsId;
jmethodID g_endResultsId;
// Cached classes and methods to return results.
jclass g_resultClass;
jmethodID g_resultConstructor;
jmethodID g_suggestConstructor;
jclass g_descriptionClass;
jmethodID g_descriptionConstructor;

// Implements 'NativeMapSearchListener' java interface.
jmethodID g_mapResultsMethod;
jclass g_mapResultClass;
jmethodID g_mapResultCtor;

jobject ToJavaResult(Result & result, bool hasPosition, double lat, double lon)
{
  JNIEnv * env = jni::GetEnv();
  ::Framework * fr = g_framework->NativeFramework();

  jni::TScopedLocalIntArrayRef ranges(env, env->NewIntArray(result.GetHighlightRangesCount() * 2));
  jint * rawArr = env->GetIntArrayElements(ranges, nullptr);
  for (int i = 0; i < result.GetHighlightRangesCount(); i++)
  {
    auto const & range = result.GetHighlightRange(i);
    rawArr[2 * i] = range.first;
    rawArr[2 * i + 1] = range.second;
  }
  env->ReleaseIntArrayElements(ranges.get(), rawArr, 0);

  ms::LatLon ll = ms::LatLon::Zero();
  string distance;
  if (result.HasPoint())
  {
    ll = MercatorBounds::ToLatLon(result.GetFeatureCenter());
    if (hasPosition)
    {
      double dummy;
      (void) fr->GetDistanceAndAzimut(result.GetFeatureCenter(), lat, lon, 0, distance, dummy);
    }
  }

  if (result.IsSuggest())
  {
    jni::TScopedLocalRef name(env, jni::ToJavaString(env, result.GetString()));
    jni::TScopedLocalRef suggest(env, jni::ToJavaString(env, result.GetSuggestionString()));
    jobject ret = env->NewObject(g_resultClass, g_suggestConstructor, name.get(), suggest.get(), ll.lat, ll.lon, ranges.get());
    ASSERT(ret, ());
    return ret;
  }

  jni::TScopedLocalRef featureType(env, jni::ToJavaString(env, result.GetFeatureType()));
  jni::TScopedLocalRef address(env, jni::ToJavaString(env, result.GetAddress()));
  jni::TScopedLocalRef dist(env, jni::ToJavaString(env, distance));
  jni::TScopedLocalRef cuisine(env, jni::ToJavaString(env, result.GetCuisine()));
  jni::TScopedLocalRef rating(env, jni::ToJavaString(env, result.GetHotelRating()));
  jni::TScopedLocalRef pricing(env, jni::ToJavaString(env, result.GetHotelApproximatePricing()));
  jni::TScopedLocalRef desc(env, env->NewObject(g_descriptionClass, g_descriptionConstructor,
                                                featureType.get(), address.get(),
                                                dist.get(), cuisine.get(),
                                                rating.get(), pricing.get(),
                                                result.GetStarsCount(),
                                                static_cast<jint>(result.IsOpenNow())));

  jni::TScopedLocalRef name(env, jni::ToJavaString(env, result.GetString()));
  jobject ret = env->NewObject(g_resultClass, g_resultConstructor, name.get(), desc.get(), ll.lat, ll.lon, ranges.get());
  ASSERT(ret, ());

  return ret;
}

jobjectArray BuildJavaResults(Results const & results, bool hasPosition, double lat, double lon)
{
  JNIEnv * env = jni::GetEnv();

  g_results = results;

  int const count = g_results.GetCount();
  jobjectArray const jResults = env->NewObjectArray(count, g_resultClass, nullptr);
  for (int i = 0; i < count; i++)
  {
    jni::TScopedLocalRef jRes(env, ToJavaResult(g_results.GetResult(i), hasPosition, lat, lon));
    env->SetObjectArrayElement(jResults, i, jRes.get());
  }
  return jResults;
}

void OnResults(Results const & results, long long timestamp, bool isMapAndTable,
               bool hasPosition, double lat, double lon)
{
  // Ignore results from obsolete searches.
  if (g_queryTimestamp > timestamp)
    return;

  JNIEnv * env = jni::GetEnv();

  if (results.IsEndMarker())
  {
    env->CallVoidMethod(g_javaListener, g_endResultsId, static_cast<jlong>(timestamp));
    if (isMapAndTable && results.IsEndedNormal())
      g_framework->NativeFramework()->UpdateUserViewportChanged();
    return;
  }

  jni::TScopedLocalObjectArrayRef jResults(env, BuildJavaResults(results, hasPosition, lat, lon));
  env->CallVoidMethod(g_javaListener, g_updateResultsId, jResults.get(), static_cast<jlong>(timestamp));
}

jobjectArray BuildJavaMapResults(vector<storage::DownloaderSearchResult> const & results)
{
  JNIEnv * env = jni::GetEnv();

  int const count = results.size();
  jobjectArray const res = env->NewObjectArray(count, g_mapResultClass, nullptr);
  for (int i = 0; i < count; i++)
  {
    jni::TScopedLocalRef country(env, jni::ToJavaString(env, results[i].m_countryId));
    jni::TScopedLocalRef matched(env, jni::ToJavaString(env, results[i].m_matchedName));
    jni::TScopedLocalRef item(env, env->NewObject(g_mapResultClass, g_mapResultCtor, country.get(), matched.get()));
    env->SetObjectArrayElement(res, i, item.get());
  }

  return res;
}

void OnMapSearchResults(storage::DownloaderSearchResults const & results, long long timestamp)
{
  // Ignore results from obsolete searches.
  if (g_queryTimestamp > timestamp)
    return;

  JNIEnv * env = jni::GetEnv();
  jni::TScopedLocalObjectArrayRef jResults(env, BuildJavaMapResults(results.m_results));
  env->CallVoidMethod(g_javaListener, g_mapResultsMethod, jResults.get(), static_cast<jlong>(timestamp), results.m_endMarker);
}

}  // namespace

extern "C"
{
  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_search_SearchEngine_nativeInit(JNIEnv * env, jobject thiz)
  {
    g_javaListener = env->NewGlobalRef(thiz);
    g_updateResultsId = jni::GetMethodID(env, g_javaListener, "onResultsUpdate", "([Lcom/mapswithme/maps/search/SearchResult;J)V");
    g_endResultsId = jni::GetMethodID(env, g_javaListener, "onResultsEnd", "(J)V");
    g_resultClass = jni::GetGlobalClassRef(env, "com/mapswithme/maps/search/SearchResult");
    g_resultConstructor = jni::GetConstructorID(env, g_resultClass, "(Ljava/lang/String;Lcom/mapswithme/maps/search/SearchResult$Description;DD[I)V");
    g_suggestConstructor = jni::GetConstructorID(env, g_resultClass, "(Ljava/lang/String;Ljava/lang/String;DD[I)V");
    g_descriptionClass = jni::GetGlobalClassRef(env, "com/mapswithme/maps/search/SearchResult$Description");
    g_descriptionConstructor = jni::GetConstructorID(env, g_descriptionClass, "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;II)V");

    g_mapResultsMethod = jni::GetMethodID(env, g_javaListener, "onMapSearchResults", "([Lcom/mapswithme/maps/search/NativeMapSearchListener$Result;JZ)V");
    g_mapResultClass = jni::GetGlobalClassRef(env, "com/mapswithme/maps/search/NativeMapSearchListener$Result");
    g_mapResultCtor = jni::GetConstructorID(env, g_mapResultClass, "(Ljava/lang/String;Ljava/lang/String;)V");

    g_hotelsFilterBuilder.Init(env);
  }

  JNIEXPORT jboolean JNICALL Java_com_mapswithme_maps_search_SearchEngine_nativeRunSearch(
      JNIEnv * env, jclass clazz, jbyteArray bytes, jstring lang, jlong timestamp,
      jboolean hasPosition, jdouble lat, jdouble lon, jobject hotelsFilter)
  {
    search::EverywhereSearchParams params;
    params.m_query = jni::ToNativeString(env, bytes);
    params.m_inputLocale = ReplaceDeprecatedLanguageCode(jni::ToNativeString(env, lang));
    params.m_onResults = bind(&OnResults, _1, timestamp, false, hasPosition, lat, lon);
    params.m_hotelsFilter = g_hotelsFilterBuilder.Build(env, hotelsFilter);

    bool const searchStarted = g_framework->NativeFramework()->SearchEverywhere(params);
    if (searchStarted)
      g_queryTimestamp = timestamp;
    return searchStarted;
  }

  JNIEXPORT void JNICALL Java_com_mapswithme_maps_search_SearchEngine_nativeRunInteractiveSearch(
      JNIEnv * env, jclass clazz, jbyteArray bytes, jstring lang, jlong timestamp,
      jboolean isMapAndTable, jobject hotelsFilter)
  {
    search::ViewportSearchParams vparams;
    vparams.m_query = jni::ToNativeString(env, bytes);
    vparams.m_inputLocale = ReplaceDeprecatedLanguageCode(jni::ToNativeString(env, lang));
    vparams.m_hotelsFilter = g_hotelsFilterBuilder.Build(env, hotelsFilter);

    // TODO (@alexzatsepin): set up vparams.m_onCompleted here and use
    // HotelsClassifier for hotel queries detection.
    g_framework->NativeFramework()->SearchInViewport(vparams);

    if (isMapAndTable)
    {
      search::EverywhereSearchParams eparams;
      eparams.m_query = vparams.m_query;
      eparams.m_inputLocale = vparams.m_inputLocale;
      eparams.m_onResults = bind(&OnResults, _1, timestamp, isMapAndTable, false /* hasPosition */,
                                 0.0 /* lat */, 0.0 /* lon */);
      eparams.m_hotelsFilter = vparams.m_hotelsFilter;
      if (g_framework->NativeFramework()->SearchEverywhere(eparams))
        g_queryTimestamp = timestamp;
    }
  }

  JNIEXPORT void JNICALL Java_com_mapswithme_maps_search_SearchEngine_nativeRunSearchMaps(
      JNIEnv * env, jclass clazz, jbyteArray bytes, jstring lang, jlong timestamp)
  {
    storage::DownloaderSearchParams params;
    params.m_query = jni::ToNativeString(env, bytes);
    params.m_inputLocale = ReplaceDeprecatedLanguageCode(jni::ToNativeString(env, lang));
    params.m_onResults = bind(&OnMapSearchResults, _1, timestamp);

    if (g_framework->NativeFramework()->SearchInDownloader(params))
      g_queryTimestamp = timestamp;
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_search_SearchEngine_nativeShowResult(JNIEnv * env, jclass clazz, jint index)
  {
    g_framework->NativeFramework()->ShowSearchResult(g_results.GetResult(index));
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_search_SearchEngine_nativeShowAllResults(JNIEnv * env, jclass clazz)
  {
    g_framework->NativeFramework()->ShowSearchResults(g_results);
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_search_SearchEngine_nativeCancelInteractiveSearch(JNIEnv * env, jclass clazz)
  {
    GetPlatform().RunOnGuiThread([]()
    {
      g_framework->NativeFramework()->CancelSearch(search::Mode::Viewport);
    });
  }
} // extern "C"
