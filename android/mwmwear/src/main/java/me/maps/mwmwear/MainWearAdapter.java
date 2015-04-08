package me.maps.mwmwear;


import android.app.Activity;
import android.app.Fragment;
import android.support.wearable.view.FragmentGridPagerAdapter;
import android.util.Log;

import me.maps.mwmwear.fragment.ArrowFragment;
import me.maps.mwmwear.fragment.MapFragment;
import me.maps.mwmwear.fragment.SearchFragment;
import me.maps.mwmwear.fragment.VoiceFragment;

public class MainWearAdapter extends FragmentGridPagerAdapter
{
  public static final int MAP_FRAGMENT = 0;
  public static final int VOICE_FRAGMENT = 1;
  public static final int SEARCH_FRAGMENT = 2;
  public static final int ARROW_FRAGMENT = 3;
  private static final String TAG = "Wear";
  private Fragment[] mFragments = new Fragment[4];

  private final Activity mActivity;

  MainWearAdapter(Activity activity)
  {
    super(activity.getFragmentManager());
    mActivity = activity;
  }

  @Override
  public int getRowCount()
  {
    return 1;
  }

  @Override
  public int getColumnCount(int i)
  {
    return 4;
  }

  @Override
  public Fragment getFragment(int row, int column)
  {
    Log.d(TAG, "Get fragment row : " + row + " column : " + column);
    initFragment(column);
    return mFragments[column];
  }

  private void initFragment(int index)
  {
    Fragment current = mFragments[index];
    if (current == null)
    {
      switch (index)
      {
      case MAP_FRAGMENT:
        current = new MapFragment();
        break;
      case VOICE_FRAGMENT:
        current = new VoiceFragment();
        break;
      case SEARCH_FRAGMENT:
        current = new SearchFragment();
        break;
      case ARROW_FRAGMENT:
        current = new ArrowFragment();
        break;
      }
      mFragments[index] = current;
    }
  }

//  @Override
//  public Drawable getBackgroundForPage(int row, int column)
//  {
//    int resId = 0;
//    switch (column)
//    {
//    case 0:
////      resId = R.drawable.common_ic_googleplayservices;
//      break;
//    case 1:
////      resId = R.drawable.close_button;
//      break;
//    case 2:
////      resId = R.drawable.common_ic_googleplayservices;
//    }
//
//    return mActivity.getDrawable(resId);
//  }

  public MapFragment getMapFragment()
  {
    initFragment(MAP_FRAGMENT);
    return (MapFragment) mFragments[MAP_FRAGMENT];
  }
}
