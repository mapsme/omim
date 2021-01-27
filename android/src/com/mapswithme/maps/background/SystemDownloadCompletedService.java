package com.mapswithme.maps.background;

import android.app.DownloadManager;
import android.content.Intent;
import android.database.Cursor;

import androidx.annotation.NonNull;
import androidx.core.app.JobIntentService;
import com.mapswithme.maps.MwmApplication;
import com.mapswithme.maps.bookmarks.BookmarksDownloadCompletedProcessor;
import com.mapswithme.maps.downloader.MapDownloadCompletedProcessor;
import com.mapswithme.util.Utils;

public class SystemDownloadCompletedService extends JobIntentService
{
  @Override
  public void onCreate()
  {
    super.onCreate();
    MwmApplication app = (MwmApplication) getApplication();
    if (app.arePlatformAndCoreInitialized())
      return;
    app.initCore();
  }

  @Override
  protected void onHandleWork(@NonNull Intent intent)
  {
    DownloadManager manager = (DownloadManager) getSystemService(DOWNLOAD_SERVICE);
    if (manager == null)
      throw new IllegalStateException("Failed to get a download manager");

    processIntent(manager, intent);
  }

  private void processIntent(@NonNull DownloadManager manager, @NonNull Intent intent)
  {
    Cursor cursor = null;
    try
    {
      final long id = intent.getLongExtra(DownloadManager.EXTRA_DOWNLOAD_ID, 0);
      DownloadManager.Query query = new DownloadManager.Query().setFilterById(id);
      cursor = manager.query(query);
      if (!cursor.moveToFirst())
        return;

      if (MapDownloadCompletedProcessor.isSupported(cursor))
        MapDownloadCompletedProcessor.process(getApplicationContext(), id, cursor);
      else
        BookmarksDownloadCompletedProcessor.process(getApplicationContext(), cursor);
    }
    finally
    {
      Utils.closeSafely(cursor);
    }
  }
}
