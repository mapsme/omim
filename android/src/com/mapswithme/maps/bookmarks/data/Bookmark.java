package com.mapswithme.maps.bookmarks.data;

import android.annotation.SuppressLint;
import android.os.Parcel;
import android.support.annotation.IntRange;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.text.TextUtils;

import com.mapswithme.maps.Framework;
import com.mapswithme.maps.ads.Banner;
import com.mapswithme.maps.ads.LocalAdInfo;
import com.mapswithme.maps.routing.RoutePointInfo;
import com.mapswithme.maps.taxi.TaxiManager;
import com.mapswithme.util.Constants;

// TODO consider refactoring to remove hack with MapObject unmarshalling itself and Bookmark at the same time.
@SuppressLint("ParcelCreator")
public class Bookmark extends MapObject
{
  private final Icon mIcon;
  private int mCategoryId;
  private int mBookmarkId;
  private double mMerX;
  private double mMerY;
  @Nullable
  private final String mObjectTitle;

  public Bookmark(@NonNull FeatureId featureId, @IntRange(from = 0) int categoryId,
                  @IntRange(from = 0) int bookmarkId,
                  String title, @Nullable String secondaryTitle, @Nullable String objectTitle,
                  @Nullable Banner[] banners, @TaxiManager.TaxiType int[] reachableByTaxiTypes,
                  @Nullable String bookingSearchUrl, @Nullable LocalAdInfo localAdInfo,
                  @Nullable RoutePointInfo routePointInfo)
  {
    super(featureId, BOOKMARK, title, secondaryTitle, "", "", 0, 0, "",
          banners, reachableByTaxiTypes, bookingSearchUrl, localAdInfo, routePointInfo);

    mCategoryId = categoryId;
    mBookmarkId = bookmarkId;
    mIcon = getIconInternal();
    initXY();
    mObjectTitle = objectTitle;
  }

  private void initXY()
  {
    final ParcelablePointD ll = nativeGetXY(mCategoryId, mBookmarkId);
    mMerX = ll.x;
    mMerY = ll.y;

    setLat(Math.toDegrees(2.0 * Math.atan(Math.exp(Math.toRadians(ll.y))) - Math.PI / 2.0));
    setLon(ll.x);
  }

  @Override
  public void writeToParcel(Parcel dest, int flags)
  {
    super.writeToParcel(dest, flags);
    dest.writeInt(mCategoryId);
    dest.writeInt(mBookmarkId);
    dest.writeString(mObjectTitle);
  }

  protected Bookmark(@MapObjectType int type, Parcel source)
  {
    super(type, source);
    mCategoryId = source.readInt();
    mBookmarkId = source.readInt();
    mIcon = getIconInternal();
    initXY();
    mObjectTitle = source.readString();
  }

  @Override
  public String getAddress()
  {
    return nativeGetAddress(mCategoryId, mBookmarkId);
  }

  @Override
  public double getScale()
  {
    return nativeGetScale(mCategoryId, mBookmarkId);
  }

  public DistanceAndAzimut getDistanceAndAzimuth(double cLat, double cLon, double north)
  {
    return Framework.nativeGetDistanceAndAzimuth(mMerX, mMerY, cLat, cLon, north);
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
  public String getSubtitle()
  {
    String subtitle = getCategoryName();
    if (!TextUtils.isEmpty(mObjectTitle) && !mTitle.equals(mObjectTitle))
      subtitle += " - " + mObjectTitle;
    return subtitle;
  }

  public String getCategoryName()
  {
    return getCategory().getName();
  }

  @NonNull
  private BookmarkCategory getCategory()
  {
    return BookmarkManager.INSTANCE.getCategory(mCategoryId);
  }

  public void setCategoryId(@IntRange(from = 0) int catId)
  {
    if (catId == mCategoryId)
      return;

    mBookmarkId = nativeChangeCategory(mCategoryId, catId, mBookmarkId);
    mCategoryId = catId;
  }

  public void setParams(String title, Icon icon, String description)
  {
    if (icon == null)
      icon = mIcon;

    if (!title.equals(getTitle()) || icon != mIcon || !description.equals(getBookmarkDescription()))
    {
      nativeSetBookmarkParams(mCategoryId, mBookmarkId, title, icon != null ? icon.getType() : "",
                              description);
      mTitle = title;
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

  private native String nativeGetAddress(@IntRange(from = 0) int catId, @IntRange(from = 0) long bookmarkId);

  private native String nativeEncode2Ge0Url(@IntRange(from = 0) int catId, @IntRange(from = 0) long bookmarkId, boolean addName);

  private native void nativeSetBookmarkParams(@IntRange(from = 0) int catId, @IntRange(from = 0) long bookmarkId, String name, String type, String descr);

  private native int nativeChangeCategory(@IntRange(from = 0) int oldCatId, @IntRange(from = 0) int newCatId, @IntRange(from = 0) long bookmarkId);
}
