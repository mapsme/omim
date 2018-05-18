package com.mapswithme.maps.bookmarks;

import android.content.Context;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.FragmentPagerAdapter;

import java.util.List;

public class BookmarksPagerAdapter extends FragmentPagerAdapter
{
  private final List<BookmarksPageFactory> mFactories;
  private final Context mContext;

  public BookmarksPagerAdapter(Context context, FragmentManager fm, List<BookmarksPageFactory> factories)
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
    return mContext.getResources().getString(mFactories.get(position).getTitle());
  }

  @Override
  public int getCount()
  {
    return mFactories.size();
  }
}
