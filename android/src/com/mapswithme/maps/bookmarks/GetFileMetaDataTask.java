package com.mapswithme.maps.bookmarks;

import android.app.DownloadManager;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.AsyncTask;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import com.mapswithme.maps.bookmarks.data.BookmarkManager;
import com.mapswithme.util.Utils;

import java.io.IOException;

class GetFileMetaDataTask extends AsyncTask<Intent, Void, OperationStatus<GetFileMetaDataTask.Result,
    GetFileMetaDataTask.Error>>
{
  @NonNull
  private final DownloadManager mDownloadManager;

  private GetFileMetaDataTask(@NonNull DownloadManager downloadManager)
  {
    mDownloadManager = downloadManager;
  }

  @Override
  protected OperationStatus<Result, Error> doInBackground(Intent... intents)
  {
    Intent intent = intents[0];
    try
    {
      Result result = doInBackgroundInternal(mDownloadManager, intent);
      return new OperationStatus<>(result, null);
    }
    catch (IOException e)
    {
      return new OperationStatus<>(null, new Error(e));
    }
  }

  @Override
  protected void onPostExecute(OperationStatus<Result, Error> status)
  {
    if (status.isOk())
    {
      Result result = status.getResult();
      BookmarkManager.INSTANCE.importFromCatalog(result.getArchiveId(), result.getFilePath());
    }
  }

  public static GetFileMetaDataTask create(DownloadManager manager)
  {
    return new GetFileMetaDataTask(manager);
  }

  public static class Result
  {
    @Nullable
    private final String mFilePath;
    @Nullable
    private final String mArchiveId;

    public Result(@Nullable String filePath, @Nullable String archiveId)
    {
      mFilePath = filePath;
      mArchiveId = archiveId;
    }

    @Nullable
    public String getFilePath()
    {
      return mFilePath;
    }

    @Nullable
    public String getArchiveId()
    {
      return mArchiveId;
    }
  }

  public static class Error
  {
    @Nullable
    private Throwable mThrowable;

    public Error(@Nullable Throwable throwable)
    {
      mThrowable = throwable;
    }

    @Nullable
    public Throwable getThrowable()
    {
      return mThrowable;
    }
  }

  private Result doInBackgroundInternal(@NonNull DownloadManager manager,
                                        @NonNull Intent intent) throws IOException
  {
    Cursor cursor = null;
    try
    {
      cursor = openCursor(manager, intent);
      String filePath = getFilePath(cursor);
      String archiveId = getArchiveId(cursor);
      return new Result(filePath, archiveId);
    }
    finally
    {
      Utils.closeSafely(cursor);
    }
  }

  private String getFilePath(Cursor cursor)
  {
    return getColumnValue(cursor, DownloadManager.COLUMN_LOCAL_FILENAME);
  }

  private String getArchiveId(Cursor cursor)
  {
    return Uri.parse(getColumnValue(cursor, DownloadManager.COLUMN_URI)).getLastPathSegment();
  }

  @Nullable
  private String getColumnValue(@NonNull Cursor cursor, @NonNull String columnName)
  {
    int status = cursor.getInt(cursor.getColumnIndex(DownloadManager.COLUMN_STATUS));
    return status == DownloadManager.STATUS_SUCCESSFUL
           ? cursor.getString(cursor.getColumnIndex(columnName))
           : null;
  }

  private Cursor openCursor(@NonNull DownloadManager manager,
                            @NonNull Intent intent) throws IOException
  {
    final long id = intent.getLongExtra(DownloadManager.EXTRA_DOWNLOAD_ID, 0);
    DownloadManager.Query query = new DownloadManager.Query().setFilterById(id);
    Cursor cursor = manager.query(query);
    if (cursor.moveToFirst())
    {
      return cursor;
    }
    else
    {
      throw new IOException("cursor cannot move to first");
    }
  }
}
