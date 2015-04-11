package me.maps.mwmwear;

import android.app.Activity;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.support.wearable.view.GridPagerAdapter;
import android.support.wearable.view.GridViewPager;
import android.support.wearable.view.WatchViewStub;
import android.util.Log;

import me.maps.mwmwear.communication.WearableManager;

public class WearMwmActivity extends Activity
{
  private static final String TAG = "Wear";

  private GridViewPager mGrid;
  private MainWearAdapter mAdapter;
  private WearableManager mWearableManager;

  @Override
  protected void onCreate(Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);
    mWearableManager = new WearableManager(WearMwmActivity.this);
    setContentView(R.layout.activity_main);
    final WatchViewStub stub = (WatchViewStub) findViewById(R.id.watch_view_stub);
    stub.setRectLayout(R.layout.rect_activity_main);
    stub.setRoundLayout(R.layout.round_activity_main);
    stub.setOnLayoutInflatedListener(new WatchViewStub.OnLayoutInflatedListener()
    {
      @Override
      public void onLayoutInflated(WatchViewStub stub)
      {
        mGrid = (GridViewPager) stub.findViewById(R.id.vp__main);
        mGrid.setAdapter(getAdapter());
        mGrid.setOnPageChangeListener(new GridViewPager.OnPageChangeListener() {
          @Override
          public void onPageScrolled(int i, int i1, float v, float v1, int i2, int i3)
          {

          }

          @Override
          public void onPageSelected(int row, int column)
          {
            Log.d(TAG, "Grid column selected : " + column);
            if (column == MainWearAdapter.MAP_FRAGMENT)
              mWearableManager.requestMapUpdate();
          }

          @Override
          public void onPageScrollStateChanged(int i)
          {

          }
        });
      }
    });
  }

  private GridPagerAdapter getAdapter()
  {
    if (mAdapter == null)
      mAdapter = new MainWearAdapter(this);

    return mAdapter;
  }

  @Override
  protected void onResume()
  {
    super.onResume();
    mWearableManager.startCommunication();
  }

  @Override
  protected void onPause()
  {
    super.onPause();
    mWearableManager.endCommunication();
  }

  public void setMapBitmap(final Bitmap bitmap)
  {
    mGrid.post(new Runnable()
    {
      @Override
      public void run()
      {
        mAdapter.getMapFragment().setBitmap(bitmap);
      }
    });
  }

  public WearableManager getWearableManager()
  {
    return mWearableManager;
  }

  // TODO this is hack to add fragments to view pager dynamically & navigating to new fragment.
  // check https://code.google.com/p/android/issues/detail?id=75309
  public void resetAdapter()
  {
    mGrid.setAdapter(getAdapter());

    Runnable dirtyHack = new Runnable()
    {
      @Override
      public void run() {
        mGrid.setCurrentItem(0, MainWearAdapter.ARROW_FRAGMENT);
      }
    };
    mGrid.postDelayed(dirtyHack, 100);
  }
}
