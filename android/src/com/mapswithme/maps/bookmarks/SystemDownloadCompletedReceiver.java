package com.mapswithme.maps.bookmarks;

import android.app.DownloadManager;
import android.content.Context;
import android.content.Intent;
import android.support.annotation.NonNull;

import com.mapswithme.maps.background.AbstractLogBroadcastReceiver;
import com.mapswithme.maps.bookmarks.persistence.Query;
import com.mapswithme.maps.bookmarks.persistence.SystemDownloadCompletedService;

public class SystemDownloadCompletedReceiver extends AbstractLogBroadcastReceiver
{
  @NonNull
  @Override
  protected String getAssertAction()
  {
    return DownloadManager.ACTION_DOWNLOAD_COMPLETE;
  }

  @Override
  public void onReceiveInternal(@NonNull Context context, @NonNull Intent src)
  {
    DownloadManager manager = (DownloadManager) context.getSystemService(Context.DOWNLOAD_SERVICE);
    if (manager == null)
      return;

    Query request = new Query();

    Intent intent = SystemDownloadCompletedService.makeIntent(src, request);
    intent.setClass(context, SystemDownloadCompletedService.class);
    context.startService(intent);
  }
}
