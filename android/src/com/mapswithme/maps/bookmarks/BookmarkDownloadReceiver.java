package com.mapswithme.maps.bookmarks;

import android.app.Application;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.localbroadcastmanager.content.LocalBroadcastManager;
import android.text.TextUtils;

import com.mapswithme.maps.background.AbstractLogBroadcastReceiver;
import com.mapswithme.maps.base.Detachable;
import com.mapswithme.maps.bookmarks.data.BookmarkManager;
import com.mapswithme.maps.bookmarks.data.Error;
import com.mapswithme.maps.bookmarks.data.Result;
import com.mapswithme.util.log.Logger;
import com.mapswithme.util.log.LoggerFactory;

public class BookmarkDownloadReceiver extends AbstractLogBroadcastReceiver implements Detachable<BookmarkDownloadHandler>
{
  @Nullable
  private BookmarkDownloadHandler mHandler;

  @Override
  public void attach(@NonNull BookmarkDownloadHandler handler)
  {
    mHandler = handler;
  }

  @Override
  public void detach()
  {
    mHandler = null;
  }

  public void register(@NonNull Application application)
  {
    IntentFilter filter = new IntentFilter(SystemDownloadCompletedService.ACTION_DOWNLOAD_COMPLETED);
    LocalBroadcastManager.getInstance(application).registerReceiver(this, filter);
  }

  public void unregister(@NonNull Application application)
  {
    LocalBroadcastManager.getInstance(application).unregisterReceiver(this);
  }

  @NonNull
  @Override
  protected String getAssertAction()
  {
    return SystemDownloadCompletedService.ACTION_DOWNLOAD_COMPLETED;
  }

  @Override
  public void onReceiveInternal(@NonNull Context context, @NonNull Intent intent)
  {
    Logger logger = LoggerFactory.INSTANCE.getLogger(LoggerFactory.Type.BILLING);
    String tag = BookmarkDownloadReceiver.class.getSimpleName();
    OperationStatus status
        = intent.getParcelableExtra(SystemDownloadCompletedService.EXTRA_DOWNLOAD_STATUS);
    Result result = status.getResult();
    if (status.isOk() && result != null && !TextUtils.isEmpty(result.getArchiveId())
        && !TextUtils.isEmpty(result.getFilePath()))
    {
      logger.i(tag, "Start to import downloaded bookmark");
      BookmarkManager.INSTANCE.importFromCatalog(result.getArchiveId(), result.getFilePath());
      return;
    }

    logger.i(tag, "Handle download result by handler '" + mHandler + "'");

    Error error = status.getError();
    if (error == null || mHandler == null)
      return;

    if (error.isForbidden())
      mHandler.onAuthorizationRequired();
    else if (error.isPaymentRequired())
      mHandler.onPaymentRequired();
  }
}
