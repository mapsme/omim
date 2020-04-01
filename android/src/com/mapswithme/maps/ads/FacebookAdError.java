package com.mapswithme.maps.ads;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.facebook.ads.AdError;

class FacebookAdError implements NativeAdError
{
  @NonNull
  private final AdError mError;

  FacebookAdError(@NonNull AdError error)
  {
    mError = error;
  }

  @Nullable
  @Override
  public String getMessage()
  {
    return mError.getErrorMessage();
  }

  @Override
  public int getCode()
  {
    return mError.getErrorCode();
  }
}
