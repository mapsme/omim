package com.mapswithme.maps.bookmarks;

import android.view.View;

import com.mapswithme.maps.bookmarks.data.BookmarkCategory;

public interface OnItemLongClickListener<T>
{
  void onItemLongClick(View v, T t);
}
