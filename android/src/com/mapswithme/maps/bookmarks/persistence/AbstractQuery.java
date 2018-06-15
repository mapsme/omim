package com.mapswithme.maps.bookmarks.persistence;

import android.app.DownloadManager;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.widget.Toast;

import com.mapswithme.maps.MwmApplication;
import com.mapswithme.maps.bookmarks.data.BookmarkManager;
import com.mapswithme.maps.bookmarks.data.Error;
import com.mapswithme.maps.bookmarks.data.OperationStatus;
import com.mapswithme.maps.bookmarks.data.Result;
import com.mapswithme.util.StorageUtils;
import com.mapswithme.util.Utils;
import com.mapswithme.util.log.Logger;
import com.mapswithme.util.log.LoggerFactory;

import java.io.File;
import java.io.IOException;
import java.util.Collection;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

public abstract class AbstractQuery extends CatalogCategoryRequest.AbstractRequest<Collection<String>>
{
  private static final Logger LOGGER = LoggerFactory.INSTANCE.getLogger(LoggerFactory.Type.MISC);

  @NonNull
  public final Collection<String> doInBackgroundInternal(@NonNull Context context,
                                                         @NonNull Intent intent)
  {
    final long[] entries = getEntries(context, intent);
    if (entries.length == 0)
      return Collections.emptySet();

    Cursor cursor = null;
    try
    {
      cursor = getCursor(context, entries);
      return convertToResult(cursor);
    }
    finally
    {
      Utils.closeSafely(cursor);
    }
  }

  @NonNull
  private Collection<String> convertToResult(@NonNull Cursor cursor)
  {
    if (cursor.moveToFirst())
    {
      Set<String> archives = createArchives(cursor);
      return Collections.unmodifiableSet(archives);
    }
    return Collections.emptySet();
  }

  private Cursor getCursor(@NonNull Context context, @NonNull long[] entries)
  {
    DownloadManager.Query query = new DownloadManager
        .Query()
        .setFilterById(entries)
        .setFilterByStatus(DownloadManager.STATUS_SUCCESSFUL);
    DownloadManager dm = (DownloadManager) context.getSystemService(Context.DOWNLOAD_SERVICE);
    return dm.query(query);
  }

  @NonNull
  private Set<String> createArchives(@NonNull Cursor cursor)
  {
    Set<String> archives = new HashSet<>();
    do
    {
      archives.add(getArchiveId(cursor));
    } while (cursor.moveToNext());

    return archives;
  }

  @Override
  public void onPostExecute(@NonNull Context context,
                            @NonNull OperationStatus<Collection<String>, Error> status)
  {
    /*TODO special for arsentiy*/
    if (!isCoreInitialized(context))
      return;

    if (status.isOk())
      onOkResultSafely(context, status.getResult());
    else
      onErrorResult(context);
  }

  private boolean isCoreInitialized(@NonNull Context context)
  {
    MwmApplication app = (MwmApplication) context.getApplicationContext();
    return app.arePlatformAndCoreInitialized();
  }

  private void onErrorResult(@NonNull Context context)
  {
    int resId = com.mapswithme.maps.R.string.download_failed;
    Toast.makeText(context, resId, Toast.LENGTH_SHORT).show();
  }

  private void onOkResultSafely(@NonNull Context context,
                                @NonNull Collection<String> items)
  {
    try
    {
      onOkResult(context, items);
    }
    catch (IOException e)
    {
      LOGGER.d(getClass().getSimpleName(), "Cannot create archive instance");
    }
  }

  private void onOkResult(@NonNull Context context,
                          @NonNull Collection<String> items) throws IOException
  {
    BookmarkManager.INSTANCE.addCatalogListener(new ImportCatalogListener(context));
    for (String each : items)
    {
      File archive = StorageUtils.getCatalogCategoryArchive(context, each);
      BookmarkManager.INSTANCE.importFromCatalog(each, archive.getAbsolutePath());
    }
  }

  @Nullable
  private static String getArchiveId(@NonNull Cursor cursor)
  {
    return Uri.parse(getColumnValue(cursor, DownloadManager.COLUMN_URI)).getLastPathSegment();
  }

  @Nullable
  private static String getColumnValue(@NonNull Cursor cursor, @NonNull String columnName)
  {
    return cursor.getString(cursor.getColumnIndex(columnName));
  }

  public abstract long[] getEntries(@NonNull Context context, @NonNull Intent intent);

  private static class ImportCatalogListener implements BookmarkManager.BookmarksCatalogListener
  {
    @NonNull
    private final Context mContext;

    public ImportCatalogListener(@NonNull Context context)
    {
      mContext = context;
    }

    @Override
    public void onImportStarted(@NonNull String serverId)
    {

    }

    @Override
    public void onImportFinished(@NonNull String serverId, boolean successful)
    {
      BookmarkManager.INSTANCE.removeCatalogListener(this);
      if (successful)
      {
        DeleteRequest request = new DeleteRequest(serverId);
        Intent intent = SystemDownloadCompletedService.makeIntent(mContext, request);
        mContext.startService(intent);
      }
    }
  }
}
