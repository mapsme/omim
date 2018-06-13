package com.mapswithme.maps.bookmarks.data;

import android.support.annotation.NonNull;

import java.util.Collection;
import java.util.List;

public class Result
{
  @NonNull
  private Collection<String> mItems;

  public Result(@NonNull Collection<String> items)
  {
    mItems = items;
  }

  @NonNull
  public Collection<String> getItems()
  {
    return mItems;
  }
}
