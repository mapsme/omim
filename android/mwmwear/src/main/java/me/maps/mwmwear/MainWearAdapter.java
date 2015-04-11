package me.maps.mwmwear;


import android.app.Fragment;
import android.support.wearable.view.FragmentGridPagerAdapter;
import android.util.Log;

import me.maps.mwmwear.fragment.ArrowFragment;
import me.maps.mwmwear.fragment.MapFragment;
import me.maps.mwmwear.fragment.SearchAdapter;
import me.maps.mwmwear.fragment.SearchFragment;
import me.maps.mwmwear.fragment.VoiceFragment;

public class MainWearAdapter extends FragmentGridPagerAdapter implements SearchFragment.SearchResultClickListener
{
  private static final int BASE_FRAGMENT_COUNT = 3; // 3 fragments are always visible
  public static final int MAP_FRAGMENT = 0;
  public static final int VOICE_FRAGMENT = 1;
  public static final int CATEGORIES_FRAGMENT = 2;
//  public static final int SEARCH_FRAGMENT = 3; // not visible until category search is complete
  public static final int ARROW_FRAGMENT = 3; // not visible until search result is clicked
  private static final String TAG = "Wear";
  private Fragment[] mFragments = new Fragment[5];

  private final WearMwmActivity mActivity;
  private boolean mIsDirectionFragmentVisible;

  MainWearAdapter(WearMwmActivity activity)
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
    Log.d("TEST", "Get column count : " + (BASE_FRAGMENT_COUNT + (mIsDirectionFragmentVisible ? 1 : 0)));
    return BASE_FRAGMENT_COUNT + (mIsDirectionFragmentVisible ? 1 : 0);
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
      case CATEGORIES_FRAGMENT:
        final SearchFragment fragment = new SearchFragment();
        fragment.setOnSearchClickListener(this);
        current = fragment;
        break;
//      case SEARCH_FRAGMENT:

//        break;
      case ARROW_FRAGMENT:
        current = new ArrowFragment();
        break;
      }
      mFragments[index] = current;
    }
  }

  public MapFragment getMapFragment()
  {
    initFragment(MAP_FRAGMENT);
    return (MapFragment) mFragments[MAP_FRAGMENT];
  }

  @Override
  public void onSearchResultClick(SearchAdapter.SearchResult result)
  {
    mIsDirectionFragmentVisible = true;
    initFragment(ARROW_FRAGMENT);
    final ArrowFragment arrowFragment = (ArrowFragment) mFragments[ARROW_FRAGMENT];
    arrowFragment.setSearchResult(result);
    mActivity.resetAdapter();
    // TODO hide direction fragment on swipe back to search results
  }
}
