package com.mapswithme.maps.bookmarks;

import android.content.Context;
import android.support.annotation.NonNull;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.FragmentStatePagerAdapter;

import java.util.List;

public class BookmarksPagerAdapter extends FragmentStatePagerAdapter
{
  @NonNull
  private final List<BookmarksPageFactory> mFactories;
  @NonNull
  private final Context mContext;

  public BookmarksPagerAdapter(@NonNull Context context,
                               @NonNull FragmentManager fm,
                               @NonNull List<BookmarksPageFactory> factories)
  {
    super(fm);
    mContext = context.getApplicationContext();
    mFactories = factories;
  }

  @Override
  public Fragment getItem(int position)
  {
    return mFactories.get(position).instantiateFragment();
  }

  @Override
  public CharSequence getPageTitle(int position)
  {
    int titleResId = mFactories.get(position).getTitle();
    return mContext.getResources().getString(titleResId);
  }

  @Override
  public int getCount()
  {
    return mFactories.size();
  }

  @NonNull
  public BookmarksPageFactory getItemFactory(int position)
  {
    return mFactories.get(position);
  }
}
