package com.mapswithme.maps.settings;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.CheckedTextView;

import com.mapswithme.maps.R;

import java.util.ArrayList;
import java.util.List;

class StoragePathAdapter extends BaseAdapter
{
  private final LayoutInflater mInflater;

  private List<StorageItem> mItems = new ArrayList<>();
  private int mCurrentStorageIndex;
  private long mMapsSize;

  public StoragePathAdapter(Activity context)
  {
    mInflater = context.getLayoutInflater();
  }

  @Override
  public int getCount()
  {
    return mItems.size();
  }

  @Override
  public StorageItem getItem(int position)
  {
    return mItems.get(position);
  }

  @Override
  public long getItemId(int position)
  {
    return position;
  }

  @Override
  public View getView(int position, View convertView, ViewGroup parent)
  {
    if (convertView == null)
      convertView = mInflater.inflate(R.layout.item_storage, parent, false);

    final StorageItem item = mItems.get(position);
    final CheckedTextView checkedView = (CheckedTextView) convertView;
    checkedView.setText(item.mPath + ": " + StoragePathFragment.getSizeString(item.mFreeSize));
    checkedView.setChecked(position == mCurrentStorageIndex);
    checkedView.setEnabled(position == mCurrentStorageIndex || isStorageBigEnough(position));

    return convertView;
  }

  public void update(List<String> paths)
  {
    final String writablePath = StoragePathManager.getMwmPath();
    mMapsSize = StoragePathManager.getMwmDirSize();
    mItems.clear();
    for (int i = 0; i < paths.size(); i++)
    {
      final String path = paths.get(i);
      mItems.add(new StorageItem(path, StorageUtils.getFreeBytesAtPath(path)));
      if (writablePath.startsWith(path))
        mCurrentStorageIndex = i;
    }
    notifyDataSetChanged();
  }

  public boolean isStorageBigEnough(int index)
  {
    return mItems.get(index).mFreeSize >= mMapsSize;
  }

  public int getStorageIndex()
  {
    return mCurrentStorageIndex;
  }
}
