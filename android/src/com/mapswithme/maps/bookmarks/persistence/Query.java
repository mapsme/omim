package com.mapswithme.maps.bookmarks.persistence;

import android.app.DownloadManager;
import android.content.Context;
import android.content.Intent;
import android.os.Parcel;
import android.support.annotation.NonNull;

public class Query extends AbstractQuery
{
  public static final Creator<Query> CREATOR = new QueryCreator();

  @Override
  public long[] getEntries(@NonNull Context context, @NonNull Intent intent)
  {
    final long id = intent.getLongExtra(DownloadManager.EXTRA_DOWNLOAD_ID, 0);
    return new long[] { id };
  }

  public static class QueryCreator implements Creator<Query>
  {
    @Override
    public Query createFromParcel(Parcel source)
    {
      return new Query();
    }

    @Override
    public Query[] newArray(int size)
    {
      return new Query[size];
    }
  }
}
