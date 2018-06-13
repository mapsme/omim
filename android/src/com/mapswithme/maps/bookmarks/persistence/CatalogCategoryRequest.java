package com.mapswithme.maps.bookmarks.persistence;

import android.app.DownloadManager;
import android.content.Context;
import android.content.Intent;
import android.os.Parcel;
import android.os.Parcelable;
import android.support.annotation.MainThread;
import android.support.annotation.NonNull;

import com.mapswithme.maps.bookmarks.data.Error;
import com.mapswithme.util.concurrency.UiThread;

import java.io.IOException;

public interface CatalogCategoryRequest<R, E> extends Parcelable
{
  @NonNull
  OperationStatus<R, E> doInBackground(@NonNull Context context,
                                       @NonNull Intent intent);

  abstract class AbstractRequest<R> implements CatalogCategoryRequest<R, Error>
  {
    @NonNull
    @Override
    public final OperationStatus<R, Error> doInBackground(@NonNull Context context,
                                                          @NonNull Intent intent)
    {
      DownloadManager manager = (DownloadManager) context.getSystemService(Context.DOWNLOAD_SERVICE);
      if (manager == null)
      {
        return onFailedGetDownloadManager();
      }
      try
      {
        return getOperationStatus(context, intent);
      }
      catch (IOException e)
      {
        return onError(e);
      }
    }

    @NonNull
    private OperationStatus<R, Error> onError(@NonNull IOException e)
    {
      return new OperationStatus<>(null, new Error(e));
    }

    @NonNull
    private OperationStatus<R, Error> onFailedGetDownloadManager()
    {
      IllegalStateException exc = new IllegalStateException("Failed to get a download manager");
      return new OperationStatus<>(null, new Error(exc));
    }

    @NonNull
    private OperationStatus<R, Error> getOperationStatus(@NonNull Context context,
                                                         @NonNull Intent intent) throws IOException
    {
      R result = doInBackgroundInternal(context, intent);
      OperationStatus<R, Error> status = new OperationStatus<>(result, null);
      UiThread.run(new MainThreadTask<>(context, this, status));
      return status;
    }

    @Override
    public int describeContents()
    {
      return 0;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags)
    {
    }

    @MainThread
    public void onPostExecute(@NonNull Context context,
                              @NonNull OperationStatus<R, Error> status)
    {
       /*do nothing by default*/
    }

    public abstract R doInBackgroundInternal(@NonNull Context context,
                                             @NonNull Intent intent) throws IOException;
  }
}
