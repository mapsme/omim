package com.mapswithme.maps.editor;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import com.mapswithme.maps.R;
import com.mapswithme.util.StringUtils;

public class CuisineAdapter extends RecyclerView.Adapter<CuisineAdapter.ViewHolder>
{
  private static class Item implements Comparable<Item>
  {
    String cuisineTranslated;
    String cuisineKey;

    public Item(String key, String translation)
    {
      this.cuisineKey = key;
      this.cuisineTranslated = translation;
    }

    @Override
    public int compareTo(@NonNull Item another)
    {
      return cuisineTranslated.compareTo(another.cuisineTranslated);
    }
  }

  private List<Item> mItems = new ArrayList<>();
  private Set<String> mSelectedKeys = new HashSet<>();
  private String mFilter;

  public CuisineAdapter()
  {
    final String[] keys = Editor.nativeGetCuisines();
    final String[] selectedKeys = Editor.nativeGetSelectedCuisines();
    final String[] translations = Editor.nativeTranslateCuisines(keys);

    Arrays.sort(selectedKeys);
    for (int i = 0; i < keys.length; i++)
    {
      final String key = keys[i];
      mItems.add(new Item(key, translations[i]));

      if (Arrays.binarySearch(selectedKeys, key) >= 0)
        mSelectedKeys.add(key);
    }
  }

  public void setFilter(@NonNull String filter)
  {
    if (filter.equals(mFilter))
      return;
    mFilter = filter;

    final String[] filteredKeys = StringUtils.nativeFilterContainsNormalized(Editor.nativeGetCuisines(), filter);
    final String[] filteredValues = Editor.nativeTranslateCuisines(filteredKeys);

    mItems.clear();
    for (int i = 0; i < filteredKeys.length; i++)
      mItems.add(new Item(filteredKeys[i], filteredValues[i]));

    notifyDataSetChanged();
  }

  @Override
  public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType)
  {
    return new ViewHolder(LayoutInflater.from(parent.getContext()).inflate(R.layout.item_cuisine, parent, false));
  }

  @Override
  public void onBindViewHolder(ViewHolder holder, int position)
  {
    holder.bind(position);
  }

  @Override
  public int getItemCount()
  {
    return mItems.size();
  }

  public String[] getCuisines()
  {
    return mSelectedKeys.toArray(new String[mSelectedKeys.size()]);
  }

  protected class ViewHolder extends RecyclerView.ViewHolder implements CompoundButton.OnCheckedChangeListener
  {
    final TextView cuisine;
    final CheckBox selected;

    public ViewHolder(View itemView)
    {
      super(itemView);
      cuisine = (TextView) itemView.findViewById(R.id.cuisine);
      selected = (CheckBox) itemView.findViewById(R.id.selected);
      selected.setOnCheckedChangeListener(this);
      itemView.setOnClickListener(new View.OnClickListener()
      {
        @Override
        public void onClick(View v)
        {
          selected.toggle();
        }
      });
    }

    public void bind(int position)
    {
      final String text = mItems.get(position).cuisineTranslated;
      cuisine.setText(text);
      selected.setOnCheckedChangeListener(null);
      selected.setChecked(mSelectedKeys.contains(mItems.get(position).cuisineKey));
      selected.setOnCheckedChangeListener(this);
    }

    @Override
    public void onCheckedChanged(CompoundButton buttonView, boolean isChecked)
    {
      final String key = mItems.get(getAdapterPosition()).cuisineKey;
      if (isChecked)
        mSelectedKeys.add(key);
      else
        mSelectedKeys.remove(key);
    }
  }
}
