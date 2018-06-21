package com.mapswithme.maps.bookmarks;

import android.support.annotation.NonNull;
import android.view.View;

public interface OnItemClickListener<T>
{
  void onItemClick(@NonNull View v, @NonNull T item);
}
