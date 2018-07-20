package com.mapswithme.util.statistics;

import android.support.annotation.NonNull;

public class Analytics
{
  @NonNull
  private final String mName;

  public Analytics(@NonNull String name)
  {
    mName = name;
  }

  @NonNull
  public String getName()
  {
    return mName;
  }
}
