package com.mapswithme.util;

import androidx.annotation.NonNull;

public final class KeyValue
{
  @NonNull
  private final String mKey;
  @NonNull
  private final String mValue;

  public KeyValue(@NonNull String key, @NonNull String value)
  {
    mKey = key;
    mValue = value;
  }

  @NonNull
  public String getKey()
  {
    return mKey;
  }

  @NonNull
  public String getValue()
  {
    return mValue;
  }
}
