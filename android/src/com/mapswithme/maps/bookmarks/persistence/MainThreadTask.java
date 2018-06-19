package com.mapswithme.maps.bookmarks.persistence;

import android.content.Context;
import android.support.annotation.NonNull;

import com.mapswithme.maps.bookmarks.data.Error;
import com.mapswithme.maps.bookmarks.data.OperationStatus;

class MainThreadTask<R> implements Runnable
{
  @NonNull
  private final OperationStatus<R, Error> mStatus;
  @NonNull
  private Context mContext;
  @NonNull
  private final CatalogCategoryRequest.AbstractRequest<R> mFactory;

  public MainThreadTask(@NonNull Context context,
                        @NonNull CatalogCategoryRequest.AbstractRequest<R> factory,
                        @NonNull OperationStatus<R, Error> status)
  {
    mContext = context;
    mFactory = factory;
    mStatus = status;
  }

  @Override
  public void run()
  {
    mFactory.onPostExecute(mContext, mStatus);
  }
}
