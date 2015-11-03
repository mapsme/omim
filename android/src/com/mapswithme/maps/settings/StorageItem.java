package com.mapswithme.maps.settings;

import com.mapswithme.util.Constants;

public class StorageItem
{
  // Path to the root of writable directory.
  public final String mPath;
  public final long mFreeSize;

  StorageItem(String path, long size)
  {
    mPath = path;
    mFreeSize = size;
  }

  @Override
  public String toString()
  {
    return mPath + ", " + mFreeSize;
  }

  public String getMwmRootPath()
  {
    return mPath + Constants.MWM_DIR_POSTFIX;
  }
}
