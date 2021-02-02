package com.mapswithme.maps.downloader;

import android.app.DownloadManager;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.text.TextUtils;

import androidx.annotation.MainThread;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import com.mapswithme.maps.MwmApplication;
import com.mapswithme.util.Utils;

import java.io.UnsupportedEncodingException;
import java.net.URLDecoder;
import java.util.HashMap;
import java.util.Map;

public class MapDownloadManager
{
  @NonNull
  private DownloadManager mDownloadManager;
  @NonNull
  private Map<String, Long> mRestoredRequests;

  public MapDownloadManager(@NonNull Context context)
  {
    DownloadManager downloadManager =
        (DownloadManager) context.getSystemService(Context.DOWNLOAD_SERVICE);

    if (downloadManager == null)
      throw new NullPointerException("Download manager is null, failed to create MapDownloadManager");

    mDownloadManager = downloadManager;
    mRestoredRequests = loadEnqueued();
  }

  @NonNull
  private Map<String, Long> loadEnqueued()
  {
    DownloadManager.Query query = new DownloadManager.Query();
    query.setFilterByStatus(DownloadManager.STATUS_PENDING | DownloadManager.STATUS_RUNNING);
    Cursor cursor = null;
    try
    {
      cursor = mDownloadManager.query(query);

      cursor.moveToFirst();
      int count = cursor.getCount();
      Map<String, Long> result = new HashMap<>();

      for (int i = 0; i < count; ++i)
      {
        long id = cursor.getInt(cursor.getColumnIndex(DownloadManager.COLUMN_ID));
        String url = cursor.getString(cursor.getColumnIndex(DownloadManager.COLUMN_URI));
        String urlPath = getUrlPath(url);

        if (!TextUtils.isEmpty(urlPath))
          result.put(urlPath, id);

        cursor.moveToNext();
      }

      return result;
    }
    finally
    {
      Utils.closeSafely(cursor);
    }
  }

  @Nullable
  private String getUrlPath(@Nullable String url)
  {
    if (TextUtils.isEmpty(url))
      return null;

    String path = Uri.parse(url).getPath();
    if (TextUtils.isEmpty(path)|| !MapManager.nativeIsUrlSupported(path))
      return null;

    try
    {
      return URLDecoder.decode(path, "UTF-8");
    }
    catch (UnsupportedEncodingException ignored)
    {
      return null;
    }
  }

  @MainThread
  public long enqueue(@NonNull String url)
  {
    Uri uri = Uri.parse(url);
    String uriPath = uri.getPath();
    if (uriPath == null)
      throw new AssertionError("The path must be not null");

    Long id = mRestoredRequests.get(uriPath);
    long requestId = 0;

    if (id == null)
    {
      DownloadManager.Request request = new DownloadManager
          .Request(uri)
          .setNotificationVisibility(DownloadManager.Request.VISIBILITY_HIDDEN);

      requestId = mDownloadManager.enqueue(request);
    }
    else
    {
      mRestoredRequests.remove(uriPath);
      requestId = id;
    }

    return requestId;
  }

  @MainThread
  public void remove(long requestId)
  {
    mDownloadManager.remove(requestId);
  }

  @NonNull
  public static MapDownloadManager from(@NonNull Context context)
  {
    MwmApplication app = (MwmApplication) context.getApplicationContext();
    return app.getMapDownloadManager();
  }
}
