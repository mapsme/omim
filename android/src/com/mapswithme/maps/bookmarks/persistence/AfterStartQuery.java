package com.mapswithme.maps.bookmarks.persistence;

import android.content.Context;
import android.content.Intent;
import android.os.Parcel;
import android.support.annotation.NonNull;

import com.mapswithme.maps.MwmApplication;

import java.util.List;

public class AfterStartQuery extends AbstractQuery
{
  public static final Creator<AfterStartQuery> CREATOR = new QueryAfterStartCreator();

  @Override
  public long[] getEntries(@NonNull Context context, @NonNull Intent intent)
  {
    MwmApplication app = (MwmApplication) context.getApplicationContext();
    List<MwmApplication.BookmarkArchive> archives = app.getAppDb().bookmarkArchiveDao().getAll();
    return toLongArray(archives);
  }

  private static long[] toLongArray(@NonNull List<MwmApplication.BookmarkArchive> list)
  {
    long[] result = new long[list.size()];
    int i = 0;
    for (MwmApplication.BookmarkArchive each : list)
    {
      result[i++] = each.getExternalContentProviderId();
    }
    return result;
  }

  public static class QueryAfterStartCreator implements Creator<AfterStartQuery>
  {
    @Override
    public AfterStartQuery createFromParcel(Parcel source)
    {
      return new AfterStartQuery();
    }

    @Override
    public AfterStartQuery[] newArray(int size)
    {
      return new AfterStartQuery[size];
    }
  }
}
