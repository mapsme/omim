package me.maps.mwmwear.fragment;

import android.content.Context;
import android.support.v7.widget.RecyclerView;
import android.support.wearable.view.CircledImageView;
import android.support.wearable.view.WearableListView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import me.maps.mwmwear.R;
import me.maps.mwmwear.SelectableSearchLayout;

public class CategoriesAdapter extends RecyclerView.Adapter<CategoriesAdapter.ViewHolder>
{
  public static final int mCategories[] = {
      R.string.food,
      R.string.hotel,
      R.string.tourism,
      R.string.transport,
      R.string.fuel,
      R.string.shop,
      R.string.entertainment,
      R.string.atm,
      R.string.bank,
      R.string.parking,
      R.string.toilet,
      R.string.pharmacy,
      R.string.hospital,
      R.string.post,
      R.string.police,
      R.string.wifi
  };
  private static final int mIcons[] = {
      R.drawable.btn_food,
      R.drawable.btn_hotel,
      R.drawable.btn_sign,
      R.drawable.btn_transport,
      R.drawable.btn_fuel,
      R.drawable.btn_shop,
      R.drawable.btn_entertainment,
      R.drawable.btn_atm,
      R.drawable.btn_bank,
      R.drawable.btn_parking,
      R.drawable.btn_toilet,
      R.drawable.btn_pharmacy,
      R.drawable.btn_hospital,
      R.drawable.btn_post,
      R.drawable.btn_police,
      R.drawable.btn_wifi
  };

  private final LayoutInflater mInflater;

  public CategoriesAdapter(SearchFragment fragment)
  {
    mInflater = fragment.getActivity().getLayoutInflater();
  }

  @Override
  public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType)
  {
    return new ViewHolder(new SelectableSearchLayout(parent.getContext()));
  }

  @Override
  public void onBindViewHolder(ViewHolder holder, int position)
  {
    holder.mName.setText(mCategories[position]);
    holder.mIcon.setImageResource(mIcons[position]);
  }

  @Override
  public long getItemId(int position)
  {
    return position;
  }

  @Override
  public int getItemCount()
  {
    return mCategories.length;
  }

  public static class ViewHolder extends WearableListView.ViewHolder
  {
    public TextView mName;
    public CircledImageView mIcon;

    public ViewHolder(View v)
    {
      super(v);
      mName = (TextView) v.findViewById(R.id.tv__category);
      mIcon = (CircledImageView) v.findViewById(R.id.iv__category);
    }

    public String getCategoryQuery(Context context)
    {
      return context.getString(mCategories[getPosition()]) + ' ';
    }
  }
}