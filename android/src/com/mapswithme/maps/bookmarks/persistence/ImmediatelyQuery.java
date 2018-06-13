package com.mapswithme.maps.bookmarks.persistence;

import android.app.DownloadManager;
import android.content.Context;
import android.content.Intent;
import android.os.Parcel;
import android.support.annotation.NonNull;

public class ImmediatelyQuery extends AbstractQuery
{
  public static final Creator<ImmediatelyQuery> CREATOR = new QueryCreator();

  @Override
  public long[] getEntries(@NonNull Context context, @NonNull Intent intent)
  {
    final long id = intent.getLongExtra(DownloadManager.EXTRA_DOWNLOAD_ID, 0);
    return new long[] { id };
  }

  public static class QueryCreator implements Creator<ImmediatelyQuery>
  {
    @Override
    public ImmediatelyQuery createFromParcel(Parcel source)
    {
      return new ImmediatelyQuery();
    }

    @Override
    public ImmediatelyQuery[] newArray(int size)
    {
      return new ImmediatelyQuery[size];
    }
  }
}
