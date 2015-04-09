package me.maps.mwmwear.fragment;

import android.support.v7.widget.RecyclerView;
import android.text.Html;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.List;

import me.maps.mwmwear.R;

public class SearchAdapter extends RecyclerView.Adapter<SearchAdapter.ViewHolder>
{
  private final LayoutInflater mInflater;
  private List<SearchResult> mResults = new ArrayList<>();

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

  @Override
  public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType)
  {
    return new ViewHolder(mInflater.inflate(R.layout.item_search, parent, false));
  }

  @Override
  public void onBindViewHolder(ViewHolder holder, int position)
  {
    final SearchResult r = mResults.get(position);
    if (r == null)
      return;

    holder.mName.setText(Html.fromHtml(r.mName));
    holder.mType.setText(r.mType);
    holder.mDistance.setText(r.mDistance);
  }

  @Override
  public int getItemCount()
  {
    return mResults.size();
  }

  public static class ViewHolder extends RecyclerView.ViewHolder
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
    public String mDistance;

    public SearchResult(String name, String amenity, String distance)
    {
      mName = name;
      mType = amenity;
      mDistance = distance;
    }
  }
}