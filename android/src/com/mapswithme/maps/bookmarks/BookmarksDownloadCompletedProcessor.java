package com.mapswithme.maps.bookmarks;

import android.app.Application;
import android.app.DownloadManager;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.WorkerThread;
import androidx.localbroadcastmanager.content.LocalBroadcastManager;
import com.mapswithme.maps.MwmApplication;
import com.mapswithme.maps.bookmarks.data.Error;
import com.mapswithme.maps.bookmarks.data.Result;
import com.mapswithme.maps.purchase.BookmarkPaymentDataParser;
import com.mapswithme.maps.purchase.PaymentDataParser;
import com.mapswithme.util.concurrency.UiThread;
import com.mapswithme.util.log.Logger;
import com.mapswithme.util.log.LoggerFactory;
import com.mapswithme.util.statistics.PushwooshHelper;

import java.io.UnsupportedEncodingException;
import java.net.URLDecoder;

public class BookmarksDownloadCompletedProcessor
{
  private static final Logger LOGGER = LoggerFactory.INSTANCE.getLogger(LoggerFactory.Type.DOWNLOADER);
  private static final String TAG = BookmarksDownloadCompletedProcessor.class.getSimpleName();

  public final static String ACTION_DOWNLOAD_COMPLETED = "action_download_completed";
  public final static String EXTRA_DOWNLOAD_STATUS = "extra_download_status";

  @WorkerThread
  public static boolean process(@NonNull Context context, long id, @NonNull Cursor cursor)
  {
    try
    {
      final OperationStatus status = processInternal(context, cursor);
      LOGGER.i(TAG, "Download status: " + status);

      UiThread.run(new BookmarkDownloadCompletedTask(context, status));
    }
    catch (Exception e)
    {
      LOGGER.e(TAG, "Failed to process bookmark download for request id " + id + ". Exception ", e);
      return false;
    }

    return true;
  }

  @NonNull
  private static OperationStatus processInternal(@NonNull Context context, @NonNull Cursor cursor)
  {
    if (isDownloadFailed(cursor))
    {
      Error error = new Error(getHttpStatus(cursor), getErrorMessage(cursor));
      return new OperationStatus(null, error);
    }

    logToPushWoosh((Application) context, cursor);

    Result result = new Result(getFilePath(cursor), getArchiveId(cursor));
    return new OperationStatus(result, null);
  }

  private static boolean isDownloadFailed(@NonNull Cursor cursor)
  {
    int status = cursor.getInt(cursor.getColumnIndex(DownloadManager.COLUMN_STATUS));
    return status != DownloadManager.STATUS_SUCCESSFUL;
  }

  @Nullable
  private static String getFilePath(@NonNull Cursor cursor)
  {
    String localUri = getColumnValue(cursor, DownloadManager.COLUMN_LOCAL_URI);
    return localUri == null ? null : Uri.parse(localUri).getPath();
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

  private static int getHttpStatus(@NonNull Cursor cursor)
  {
    String rawStatus = cursor.getString(cursor.getColumnIndex(DownloadManager.COLUMN_STATUS));
    return Integer.parseInt(rawStatus);
  }

  @Nullable
  private static String getErrorMessage(@NonNull Cursor cursor)
  {
    return cursor.getString(cursor.getColumnIndex(DownloadManager.COLUMN_REASON));
  }

  private static void logToPushWoosh(@NonNull Application application, @NonNull Cursor cursor)
  {
    String url = getColumnValue(cursor, DownloadManager.COLUMN_URI);
    if (TextUtils.isEmpty(url))
      return;

    String decodedUrl;
    try
    {
      decodedUrl = URLDecoder.decode(url, "UTF-8");
    }
    catch (UnsupportedEncodingException exception)
    {
      decodedUrl = "";
    }

    PaymentDataParser p = new BookmarkPaymentDataParser();
    String productId = p.getParameterByName(decodedUrl, BookmarkPaymentDataParser.PRODUCT_ID);
    String name = p.getParameterByName(decodedUrl, BookmarkPaymentDataParser.NAME);

    MwmApplication app = (MwmApplication) application;
    if (TextUtils.isEmpty(productId))
    {
      app.sendPushWooshTags("Bookmarks_Guides_free_title", new String[] {name});
      app.sendPushWooshTags("Bookmarks_Guides_free_date",
                            new String[] {PushwooshHelper.nativeGetFormattedTimestamp()});
    }
    else
    {
      app.sendPushWooshTags("Bookmarks_Guides_paid_tier", new String[] {productId});
      app.sendPushWooshTags("Bookmarks_Guides_paid_title", new String[] {name});
      app.sendPushWooshTags("Bookmarks_Guides_paid_date",
                            new String[] {PushwooshHelper.nativeGetFormattedTimestamp()});
    }
  }

  private static class BookmarkDownloadCompletedTask implements Runnable
  {
    @NonNull
    private final Context mAppContext;
    @NonNull
    private final OperationStatus mStatus;

    private BookmarkDownloadCompletedTask(@NonNull Context applicationContext,
                                          @NonNull OperationStatus status)
    {
      mAppContext = applicationContext;
      mStatus = status;
    }

    @Override
    public void run()
    {
      Intent intent = new Intent(ACTION_DOWNLOAD_COMPLETED);
      intent.putExtra(EXTRA_DOWNLOAD_STATUS, mStatus);
      LocalBroadcastManager.getInstance(mAppContext).sendBroadcast(intent);
    }
  }
}
