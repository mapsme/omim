package com.mapswithme.maps.bookmarks.persistence;

import android.content.Context;
import android.content.Intent;
import android.os.Parcel;
import android.support.annotation.NonNull;

import com.mapswithme.maps.MwmApplication;

import java.util.List;

public class PollingQuery extends AbstractQuery
{
  public static final Creator<PollingQuery> CREATOR = new QueryAfterStartCreator();

  @Override
  public long[] getEntries(@NonNull Context context, @NonNull Intent intent)
  {
    MwmApplication app = (MwmApplication) context.getApplicationContext();
    List<BookmarkCategoryArchive> archives = app.getAppDb().bookmarkArchiveDao().getAll();
    return toLongArray(archives);
  }

  private static long[] toLongArray(@NonNull List<BookmarkCategoryArchive> items)
  {
    long[] result = new long[items.size()];
    for (int i = 0; i < items.size(); i++)
    {
      result[i++] = items.get(i).getExternalContentProviderId();
    }
    return result;
  }

  public static class QueryAfterStartCreator implements Creator<PollingQuery>
  {
    @Override
    public PollingQuery createFromParcel(Parcel source)
    {
      return new PollingQuery();
    }

    @Override
    public PollingQuery[] newArray(int size)
    {
      return new PollingQuery[size];
    }
  }
}
