package com.mapswithme.maps.bookmarks.persistence;

import android.content.Context;
import android.content.Intent;
import android.os.Parcel;
import android.support.annotation.NonNull;

import com.mapswithme.maps.MwmApplication;
import com.mapswithme.maps.bookmarks.data.EmptyResult;

public class DeleteRequest extends CatalogCategoryRequest.AbstractRequest<EmptyResult>
{
  public static final Creator<DeleteRequest> CREATOR = new DeleteRequestCreator();

  @NonNull
  private final String mServerId;

  public DeleteRequest(@NonNull String serverId)
  {
    this.mServerId = serverId;
  }

  @Override
  public EmptyResult doInBackgroundInternal(@NonNull Context context, @NonNull Intent intent)
  {
    MwmApplication app = (MwmApplication) context.getApplicationContext();
    BookmarkArchiveDao dao = app.getAppDb().bookmarkArchiveDao();
    dao.deleteByServerId(mServerId);

    return new EmptyResult();
  }

  @Override
  public int describeContents()
  {
    return 0;
  }

  protected DeleteRequest(Parcel in)
  {
    this.mServerId = in.readString();
  }

  @Override
  public void writeToParcel(Parcel dest, int flags)
  {
    super.writeToParcel(dest, flags);
    dest.writeString(this.mServerId);
  }

  private static class DeleteRequestCreator implements Creator<DeleteRequest>
  {
    @Override
    public DeleteRequest createFromParcel(Parcel source)
    {
      return new DeleteRequest(source);
    }

    @Override
    public DeleteRequest[] newArray(int size)
    {
      return new DeleteRequest[size];
    }
  }
}
