package com.mapswithme.maps.bookmarks;

import android.content.Context;
import android.support.annotation.NonNull;

import com.mapswithme.maps.bookmarks.data.BookmarkCategory;
import com.mapswithme.maps.bookmarks.data.BookmarkManager;

import java.util.List;

public class CatalogBookmarkCategoriesAdapter extends BookmarkCategoriesAdapter
{
  CatalogBookmarkCategoriesAdapter(@NonNull Context context)
  {
    super(context, BookmarksPageFactory.CATALOG.getResProvider());
  }

  @Override
  public List<BookmarkCategory> getBookmarkCategories()
  {
    return BookmarkManager.INSTANCE.getCatalogCategoriesSnapshot().items();
  }
}
