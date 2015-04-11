package me.maps.mwmwear.fragment;

import android.app.Fragment;
import android.location.Location;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.wearable.view.WatchViewStub;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import me.maps.mwmwear.R;
import me.maps.mwmwear.Utils;
import me.maps.mwmwear.communication.WearableManager;
import me.maps.mwmwear.widget.ArrowView;

public class ArrowFragment extends Fragment
{
  private ArrowView mAvDirection;
  private TextView mTvName;
  private TextView mTvDistance;

  private SearchAdapter.SearchResult mResult;

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
  {
    final View root = inflater.inflate(R.layout.fragment_arrow, container, false);
    final WatchViewStub stub = (WatchViewStub) root.findViewById(R.id.search_view_stub);
    stub.setOnLayoutInflatedListener(new WatchViewStub.OnLayoutInflatedListener()
    {
      @Override
      public void onLayoutInflated(WatchViewStub stub)
      {
        mAvDirection = (ArrowView) stub.findViewById(R.id.av__direction);
        mTvName = (TextView) stub.findViewById(R.id.tv__title);
        mTvDistance = (TextView) stub.findViewById(R.id.tv__straight_distance);
        refreshViews();
      }
    });
    return root;
  }

  public void setSearchResult(SearchAdapter.SearchResult result)
  {
    mResult = result;
    refreshViews();
  }

  private void refreshViews()
  {
    if (mResult == null || mAvDirection == null)
      return;

    mTvName.setText(mResult.mName);
    final Location location = WearableManager.getLastLocation();
    Log.d("TEST", "Last location got : " + location);
    if (location != null)
    {
      final double distance = Utils.getDistanceMeters(location.getLatitude(), location.getLongitude(), mResult.mLat, mResult.mLon);
      mTvDistance.setText(Utils.formatDistanceInMeters(distance));
    }
    // TODO arrow direction
  }
}
