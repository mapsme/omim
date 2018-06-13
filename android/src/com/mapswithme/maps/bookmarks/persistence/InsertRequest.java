package com.mapswithme.maps.bookmarks.persistence;

import android.content.Context;
import android.content.Intent;
import android.os.Parcel;
import android.support.annotation.NonNull;

import com.mapswithme.maps.bookmarks.BookmarksDownloadManager;

public class InsertRequest extends CatalogCategoryRequest.AbstractRequest<InsertRequest.Result>
{
  public static final Creator<InsertRequest> CREATOR = new InsertCreator();

  @NonNull
  private final String mUrl;

  public InsertRequest(@NonNull String url)
  {
    mUrl = url;
  }

  protected InsertRequest(Parcel in)
  {
    this.mUrl = in.readString();
  }

  @Override
  public void writeToParcel(Parcel dest, int flags)
  {
    super.writeToParcel(dest, flags);
    dest.writeString(this.mUrl);
  }

  @Override
  public Result doInBackgroundInternal(@NonNull Context context, @NonNull Intent intent)
  {
    BookmarksDownloadManager dm = BookmarksDownloadManager.from(context);
    String serverId = dm.enqueueRequestBlocking(mUrl);
    return new Result(serverId);
  }

  public static class Result
  {

    @NonNull
    private String mServerId;

    public Result(@NonNull String serverId)
    {
      mServerId = serverId;
    }
  }

  public static class InsertCreator implements Creator<InsertRequest>
  {
    @Override
    public InsertRequest createFromParcel(Parcel source)
    {
      return new InsertRequest(source);
    }

    @Override
    public InsertRequest[] newArray(int size)
    {
      return new InsertRequest[size];
    }
  }
}
