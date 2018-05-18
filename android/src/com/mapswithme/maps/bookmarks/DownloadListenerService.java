package com.mapswithme.maps.bookmarks;

import android.app.DownloadManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;

public class DownloadListenerService extends BroadcastReceiver
{
  @Override
  public void onReceive(Context context, Intent intent)
  {

    DownloadManager downloadManager = (DownloadManager) context.getSystemService(Context.DOWNLOAD_SERVICE);
    String action = intent.getAction();
    if (action.equals(DownloadManager.ACTION_DOWNLOAD_COMPLETE)) {
      long downloadId = intent.getLongExtra(DownloadManager.EXTRA_DOWNLOAD_ID, 0);
      Cursor cursor = downloadManager.query(new DownloadManager.Query().setFilterById(downloadId));
      if (cursor.moveToFirst()) {
        int status = cursor.getInt(cursor.getColumnIndex(DownloadManager.COLUMN_STATUS));
        String uriString = cursor.getString(cursor.getColumnIndex(DownloadManager.COLUMN_LOCAL_URI));
      }

      cursor.close();
    }
  }
}
