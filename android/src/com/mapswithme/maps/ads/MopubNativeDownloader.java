package com.mapswithme.maps.ads;

import android.content.Context;
import android.location.Location;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;

import com.google.android.gms.ads.formats.NativeAdOptions;
import com.mapswithme.maps.location.LocationHelper;
import com.mapswithme.util.Language;
import com.mapswithme.util.log.Logger;
import com.mapswithme.util.log.LoggerFactory;
import com.mopub.nativeads.BaseNativeAd;
import com.mopub.nativeads.GooglePlayServicesNative;
import com.mopub.nativeads.MoPubAdRenderer;
import com.mopub.nativeads.MoPubNative;
import com.mopub.nativeads.MopubNativeAdFactory;
import com.mopub.nativeads.NativeAd;
import com.mopub.nativeads.NativeErrorCode;
import com.mopub.nativeads.RequestParameters;
import com.mopub.nativeads.StaticNativeAd;

import java.util.Collections;
import java.util.EnumSet;
import java.util.Map;

class MopubNativeDownloader extends CachingNativeAdLoader
    implements MoPubNative.MoPubNativeNetworkListener, NativeAd.MoPubNativeEventListener
{
  private final static Logger LOGGER = LoggerFactory.INSTANCE.getLogger(LoggerFactory.Type.MISC);
  private final static String TAG = MopubNativeDownloader.class.getSimpleName();

  @Nullable
  private String mBannerId;

  MopubNativeDownloader(@Nullable OnAdCacheModifiedListener listener, @Nullable AdTracker tracker)
  {
    super(tracker, listener);
  }

  @Override
  public void loadAd(@NonNull Context context, @NonNull String bannerId)
  {
    mBannerId = bannerId;
    super.loadAd(context, bannerId);
  }

  @Override
  void loadAdFromProvider(@NonNull Context context, @NonNull String bannerId)
  {
    MoPubNative nativeAd = new MoPubNative(context, bannerId, this);

    nativeAd.registerAdRenderer(new DummyRenderer());

    RequestParameters.Builder requestParameters = new RequestParameters.Builder();

    EnumSet<RequestParameters.NativeAdAsset> assetsSet =
        EnumSet.of(RequestParameters.NativeAdAsset.TITLE,
                   RequestParameters.NativeAdAsset.TEXT,
                   RequestParameters.NativeAdAsset.CALL_TO_ACTION_TEXT,
                   RequestParameters.NativeAdAsset.ICON_IMAGE);
    requestParameters.desiredAssets(assetsSet);

    Location l = LocationHelper.INSTANCE.getSavedLocation();
    if (l != null)
      requestParameters.location(l);

    String locale = Language.nativeNormalize(Language.getDefaultLocale());
    requestParameters.keywords("user_lang:" + locale);

    Map<String, Object> extras
        = Collections.singletonMap(GooglePlayServicesNative.KEY_EXTRA_AD_CHOICES_PLACEMENT,
                                   NativeAdOptions.ADCHOICES_TOP_LEFT);
    nativeAd.setLocalExtras(extras);

    nativeAd.makeRequest(requestParameters.build());
  }

  @NonNull
  @Override
  String getProvider()
  {
    return Providers.MOPUB;
  }

  @Override
  public void onNativeLoad(final NativeAd nativeAd)
  {
    nativeAd.setMoPubNativeEventListener(this);
    LOGGER.d(TAG, "onNativeLoad nativeAd = " + nativeAd);
    CachedMwmNativeAd ad = MopubNativeAdFactory.createNativeAd(nativeAd);

    if (ad != null)
      onAdLoaded(nativeAd.getAdUnitId(), ad);
  }

  @Override
  public void onNativeFail(NativeErrorCode errorCode)
  {
    LOGGER.w(TAG, "onNativeFail " + errorCode.toString());
    if (mBannerId == null)
      throw new AssertionError("A banner id must be non-null if a error is occurred");

    onError(mBannerId, getProvider(), new MopubAdError(errorCode.toString()));
  }

  @Override
  public void onImpression(View view)
  {
    LOGGER.d(TAG, "on MoPub Ad impressed");
  }

  @Override
  public void onClick(View view)
  {
    if (!TextUtils.isEmpty(mBannerId))
      onAdClicked(mBannerId);
  }

  private static class DummyRenderer implements MoPubAdRenderer<StaticNativeAd>
  {
    @NonNull
    @Override
    public View createAdView(@NonNull Context context, @Nullable ViewGroup parent)
    {
      // This method is never called, don't worry about nullness warning.
      // noinspection ConstantConditions
      return null;
    }

    @Override
    public void renderAdView(@NonNull View view, @NonNull StaticNativeAd ad)
    {
      // No op.
    }

    @Override
    public boolean supports(@NonNull BaseNativeAd nativeAd)
    {
      return true;
    }
  }
}
