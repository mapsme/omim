package me.maps.mwmwear.communication;

import android.location.Location;

import java.util.List;

import me.maps.mwmwear.fragment.SearchAdapter;

public interface SearchListener
{
  void onSearchComplete(String query, List<SearchAdapter.SearchResult> results);

  void onCategorySearchComplete(String category, List<SearchAdapter.SearchResult> results);

  void onLocationChanged(Location location);
}
