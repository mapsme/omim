package me.maps.mwmwear.fragment;

import android.location.Location;
import android.support.v7.widget.RecyclerView;
import android.support.wearable.view.WearableListView;
import android.text.Html;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.List;

import me.maps.mwmwear.R;
import me.maps.mwmwear.widget.SelectableSearchLayout;
import me.maps.mwmwear.Utils;

public class SearchAdapter extends RecyclerView.Adapter<SearchAdapter.ViewHolder>
{
  private final LayoutInflater mInflater;
  private List<SearchResult> mResults = new ArrayList<>();
  private Location mCurrentLocation;

  public SearchAdapter(SearchFragment fragment)
  {
    mInflater = fragment.getActivity().getLayoutInflater();
  }

  public void setResults(List<SearchResult> results)
  {
    mResults.clear();
    mResults.addAll(results);
    notifyDataSetChanged();
  }

  public SearchResult getSearchResult(int position)
  {
    return mResults.get(position);
  }

  @Override
  public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType)
  {
    return new ViewHolder((new SelectableSearchLayout(parent.getContext(), R.layout.item_search, R.id.iv__distance)));
  }

  @Override
  public void onBindViewHolder(ViewHolder holder, int position)
  {
    final SearchResult r = mResults.get(position);
    if (r == null)
      return;

    holder.mName.setText(Html.fromHtml(r.mName));
    holder.mType.setText(r.mType);
    holder.mDistance.setText(calculateDistance(r));
  }

  private String calculateDistance(SearchResult r)
  {
    if (mCurrentLocation == null)
      return "? m";
    final double distance = Utils.getDistanceMeters(mCurrentLocation.getLatitude(), mCurrentLocation.getLongitude(), r.mLat, r.mLon);
    return Utils.formatDistanceInMeters(distance);
  }

  @Override
  public int getItemCount()
  {
    return mResults.size();
  }

  public void setCurrentLocation(Location location)
  {
    mCurrentLocation = location;
  }

  public static class ViewHolder extends WearableListView.ViewHolder
  {
    public TextView mName;
    public TextView mType;
    public TextView mDistance;

    public ViewHolder(View v)
    {
      super(v);
      mName = (TextView) v.findViewById(R.id.tv__search_title);
      mType = (TextView) v.findViewById(R.id.tv__search_subtitle);
      mDistance = (TextView) v.findViewById(R.id.tv__distance);
    }
  }

  public static class SearchResult
  {
    public String mName;
    public String mType;
    public double mLat;
    public double mLon;

    public SearchResult(String name, String amenity)
    {
      mName = name;
      mType = amenity;
    }

    public SearchResult(String name, String amenity, double lat, double lon)
    {
      this(name, amenity);
      mLat = lat;
      mLon = lon;
    }
  }
}