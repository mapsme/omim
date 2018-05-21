package com.mapswithme.maps.bookmarks;

import android.support.annotation.NonNull;
import android.support.v4.app.Fragment;
import android.text.TextUtils;

import com.mapswithme.maps.R;

public enum BookmarksPageFactory
{
  CATALOG(new AbstractAdapterResourceProvider.Catalog())
      {
        @NonNull
        @Override
        public Fragment instantiateFragment()
        {
          return new CachedBookmarksFragment();
        }

        @Override
        public int getTitle()
        {
          return R.string.bookmarks_page_downloaded;
        }
      },
  OWNED
      {
        @NonNull
        @Override
        public Fragment instantiateFragment()
        {
          return new BookmarkCategoriesFragment();
        }

        @Override
        public int getTitle()
        {
          return R.string.bookmarks_page_my;
        }
      };

  @NonNull
  private AbstractAdapterResourceProvider mResProvider;

  BookmarksPageFactory(@NonNull AbstractAdapterResourceProvider resourceProvider)
  {
    mResProvider = resourceProvider;
  }

  BookmarksPageFactory()
  {
    this(new AbstractAdapterResourceProvider.Default());
  }

  @NonNull
  public AbstractAdapterResourceProvider getResProvider()
  {
    return mResProvider;
  }

  public static BookmarksPageFactory get(String value)
  {
    for (BookmarksPageFactory each : values())
    {
      if (TextUtils.equals(each.name(), value))
      {
        return each;
      }
    }
    throw new IllegalArgumentException(new StringBuilder()
                                           .append("not found enum instance for value = ")
                                           .append(value)
                                           .toString());
  }

  @NonNull
  public abstract Fragment instantiateFragment();

  public abstract int getTitle();

}
