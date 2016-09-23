package com.mapswithme.maps.widget.placepage;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.ImageView;
import android.widget.TextView;

import com.mapswithme.maps.R;

import java.util.ArrayList;
import java.util.List;

class FacilitiesAdapter extends BaseAdapter
{
  static final int MAX_COUNT = 6;

  private List<SponsoredHotel.FacilityType> items = new ArrayList<>();
  private boolean isShowAll = false;

  @Override
  public int getCount()
  {
    if (items.size() > MAX_COUNT && !isShowAll)
    {
      return MAX_COUNT;
    }
    return items.size();
  }

  @Override
  public Object getItem(int position)
  {
    return items.get(position);
  }

  @Override
  public long getItemId(int position)
  {
    return position;
  }

  @Override
  public View getView(int position, View convertView, ViewGroup parent)
  {
    ViewHolder holder;
    if (convertView == null)
    {
      convertView = LayoutInflater.from(parent.getContext())
                                  .inflate(R.layout.item_facility, parent, false);
      holder = new ViewHolder(convertView);
      convertView.setTag(holder);
    } else
    {
      holder = (ViewHolder) convertView.getTag();
    }

    holder.bind(items.get(position));

    return convertView;
  }

  public void setItems(
      List<SponsoredHotel.FacilityType> items)
  {
    this.items = items;
    notifyDataSetChanged();
  }

  void setShowAll(boolean showAll)
  {
    isShowAll = showAll;
    notifyDataSetChanged();
  }

  private static class ViewHolder
  {
    ImageView mIcon;
    TextView mName;

    public ViewHolder(View view)
    {
      mIcon = (ImageView) view.findViewById(R.id.iv__icon);
      mName = (TextView) view.findViewById(R.id.tv__facility);
    }

    public void bind(SponsoredHotel.FacilityType facility)
    {
//    TODO map facility key to image resource id
      mIcon.setImageResource(R.drawable.ic_entrance);
      mName.setText(facility.getName());
    }
  }
}
