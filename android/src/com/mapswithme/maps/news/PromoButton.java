package com.mapswithme.maps.news;

import android.support.annotation.NonNull;

class PromoButton
{
  @NonNull
  private final String mLabel;
  @NonNull
  private final String mLink;

  PromoButton(@NonNull String label, @NonNull String link)
  {
    mLabel = label;
    mLink = link;
  }

  @NonNull
  String getLink()
  {
    return mLink;
  }

  @NonNull
  String getLabel()
  {
    return mLabel;
  }
}
