package com.mapswithme.maps.bookmarks.data;

import android.support.annotation.IntDef;
import android.support.annotation.NonNull;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

public class BookmarkSharingResult
{
  @Retention(RetentionPolicy.SOURCE)
  @IntDef({ SUCCESS, EMPTY_CATEGORY, ARCHIVE_ERROR, FILE_ERROR })
  public @interface Code {}

  public static final int SUCCESS = 0;
  public static final int EMPTY_CATEGORY = 1;
  public static final int ARCHIVE_ERROR = 2;
  public static final int FILE_ERROR = 3;

  private final long mCategoryId;
  @Code
  private final int mCode;
  @NonNull
  private final String mSharingPath;
  @NonNull
  private final String mErrorString;

  private BookmarkSharingResult(long categoryId, @Code int code,
                                @NonNull String sharingPath,
                                @NonNull String errorString)
  {
    mCategoryId = categoryId;
    mCode = code;
    mSharingPath = sharingPath;
    mErrorString = errorString;
  }

  public long getCategoryId()
  {
    return mCategoryId;
  }

  public int getCode()
  {
    return mCode;
  }

  @NonNull
  public String getSharingPath()
  {
    return mSharingPath;
  }

  @NonNull
  public String getErrorString()
  {
    return mErrorString;
  }
}
