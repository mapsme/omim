package com.mapswithme.maps.search;

import android.content.Context;
import android.support.annotation.Nullable;

import com.google.android.gms.ads.AdListener;
import com.google.android.gms.ads.AdSize;
import com.google.android.gms.ads.search.DynamicHeightSearchAdRequest;
import com.google.android.gms.ads.search.SearchAdView;
import com.mapswithme.maps.ads.GoogleSearchAd;
import com.mapswithme.util.concurrency.UiThread;

class GoogleAdsLoader
{
  interface AdvertLoadingListener
  {
    void onLoadingFinished(SearchAdView searchAdView);
  }

  private long mLoadingDelay;
  @Nullable
  private GoogleSearchAd mGoogleSearchAd;
  @Nullable
  private Runnable mLoadingTask;

  GoogleAdsLoader(long loadingDelay)
  {
    this.mLoadingDelay = loadingDelay;
  }

  void scheduleAdsLoading(final Context context, final String query,
                          final AdvertLoadingListener loadingListener)
  {
    cancelAdsLoading();

    mGoogleSearchAd = new GoogleSearchAd();
    if (mGoogleSearchAd.getAdUnitId().isEmpty())
      return;

    mLoadingTask = new Runnable()
    {
      @Override
      public void run()
      {
        performLoading(context, query, loadingListener);
      }
    };
    UiThread.runLater(mLoadingTask, mLoadingDelay);
  }

  void cancelAdsLoading()
  {
    if (mLoadingTask != null)
    {
      UiThread.cancelDelayedTasks(mLoadingTask);
      mLoadingTask = null;
    }
  }

  private void performLoading(Context context, String query,
                              final AdvertLoadingListener loadingListener)
  {
    if (mGoogleSearchAd == null)
      throw new AssertionError("mGoogleSearchAd can't be null here");

    //TODO: Now temporal code (from Google Ads sample) is here. It will be replaced soon.

    // Create a search ad. The ad size and ad unit ID must be set before calling loadAd.
    final SearchAdView view = new SearchAdView(context);
    view.setAdSize(AdSize.SEARCH);
    view.setAdUnitId(mGoogleSearchAd.getAdUnitId());

    // Create an ad request.
    DynamicHeightSearchAdRequest.Builder builder =
      new DynamicHeightSearchAdRequest.Builder();

    // Set the query.
    builder.setQuery(query);

    // Optionally populate the ad request builder.
    builder.setAdTest(true);
    builder.setNumber(1);
    builder.setCssWidth(300); // Equivalent to "width" CSA parameter.

    // Start loading the ad.
    view.loadAd(builder.build());

    view.setAdListener(new AdListener()
    {
      @Override
      public void onAdLoaded()
      {
        mLoadingTask = null;
        loadingListener.onLoadingFinished(view);
      }

      @Override
      public void onAdFailedToLoad(int i)
      {
        mLoadingTask = null;
      }
    });
  }
}
