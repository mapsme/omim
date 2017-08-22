package com.mapswithme.maps.search;

import android.support.annotation.NonNull;

import com.google.android.gms.ads.search.SearchAdView;

public class GoogleAdsBanner implements SearchData
{
  @NonNull
  private SearchAdView mAdView;

  public GoogleAdsBanner(@NonNull SearchAdView adView)
  {
    this.mAdView = adView;
  }

  @NonNull
  public SearchAdView getAdView()
  {
    return mAdView;
  }
}
