package com.mapswithme.maps.bookmarks.data;

import android.content.Context;
import android.os.Parcel;
import android.support.annotation.IntRange;
import android.support.annotation.Nullable;

import com.mapswithme.maps.Framework;
import com.mapswithme.util.Constants;

public class Bookmark extends MapObject
{
  private final Icon mIcon;
  private int mCategoryId;
  private int mBookmarkId;
  private double mMerX;
  private double mMerY;

  Bookmark(@IntRange(from = 0) int categoryId, @IntRange(from = 0) int bookmarkId, String name)
  {
    super(BOOKMARK, name, 0, 0, "");

    mCategoryId = categoryId;
    mBookmarkId = bookmarkId;
    mName = name;
    mIcon = getIconInternal();
    initXY();
  }

  private void initXY()
  {
    final ParcelablePointD ll = nativeGetXY(mCategoryId, mBookmarkId);
    mMerX = ll.x;
    mMerY = ll.y;

    mLat = Math.toDegrees(2.0 * Math.atan(Math.exp(Math.toRadians(ll.y))) - Math.PI / 2.0);
    mLon = ll.x;
  }

  @Override
  public void writeToParcel(Parcel dest, int flags)
  {
    dest.writeInt(mCategoryId);
    dest.writeInt(mBookmarkId);
    dest.writeString(mName);
  }

  protected Bookmark(Parcel source)
  {
    this(source.readInt(), source.readInt(), source.readString());
  }

  public final Creator<Bookmark> CREATOR = new Creator<Bookmark>() {
    @Override
    public Bookmark createFromParcel(Parcel source)
    {
      return new Bookmark(source);
    }

    @Override
    public Bookmark[] newArray(int size)
    {
      return new Bookmark[size];
    }
  };

  @Override
  public double getScale()
  {
    return nativeGetScale(mCategoryId, mBookmarkId);
  }

  public DistanceAndAzimut getDistanceAndAzimuth(double cLat, double cLon, double north)
  {
    return Framework.nativeGetDistanceAndAzimut(mMerX, mMerY, cLat, cLon, north);
  }

  private Icon getIconInternal()
  {
    return BookmarkManager.getIconByType((mCategoryId >= 0) ? nativeGetIcon(mCategoryId, mBookmarkId) : "");
  }

  public Icon getIcon()
  {
    return mIcon;
  }

  @Override
  @MapObjectType
  public int getMapObjectType()
  {
    return MapObject.BOOKMARK;
  }

  @Override
  public String getPoiTypeName()
  {
    return getCategory().getName();
  }

  public String getCategoryName(Context context)
  {
    return getCategory().getName();
  }

  private @Nullable BookmarkCategory getCategory()
  {
    return BookmarkManager.INSTANCE.getCategoryById(mCategoryId);
  }

  public void setCategoryId(@IntRange(from = 0) int catId)
  {
    if (catId == mCategoryId)
      return;

    mBookmarkId = nativeChangeCategory(mCategoryId, catId, mBookmarkId);
    mCategoryId = catId;
  }

  public void setParams(String name, Icon icon, String description)
  {
    if (icon == null)
      icon = mIcon;

    if (!name.equals(getName()) || icon != mIcon || !description.equals(getBookmarkDescription()))
    {
      nativeSetBookmarkParams(mCategoryId, mBookmarkId, name, icon.getType(), description);
      mName = name;
    }
  }

  public int getCategoryId()
  {
    return mCategoryId;
  }

  public int getBookmarkId()
  {
    return mBookmarkId;
  }

  public String getBookmarkDescription()
  {
    return nativeGetBookmarkDescription(mCategoryId, mBookmarkId);
  }

  public String getGe0Url(boolean addName)
  {
    return nativeEncode2Ge0Url(mCategoryId, mBookmarkId, addName);
  }

  public String getHttpGe0Url(boolean addName)
  {
    return getGe0Url(addName).replaceFirst(Constants.Url.GE0_PREFIX, Constants.Url.HTTP_GE0_PREFIX);
  }

  private native String nativeGetBookmarkDescription(@IntRange(from = 0) int categoryId, @IntRange(from = 0) long bookmarkId);

  private native ParcelablePointD nativeGetXY(@IntRange(from = 0) int catId, @IntRange(from = 0) long bookmarkId);

  private native String nativeGetIcon(@IntRange(from = 0) int catId, @IntRange(from = 0) long bookmarkId);

  private native double nativeGetScale(@IntRange(from = 0) int catId, @IntRange(from = 0) long bookmarkId);

  private native String nativeEncode2Ge0Url(@IntRange(from = 0) int catId, @IntRange(from = 0) long bookmarkId, boolean addName);

  private native void nativeSetBookmarkParams(@IntRange(from = 0) int catId, @IntRange(from = 0) long bookmarkId, String name, String type, String descr);

  private native int nativeChangeCategory(@IntRange(from = 0) int oldCatId, @IntRange(from = 0) int newCatId, @IntRange(from = 0) long bookmarkId);
}
