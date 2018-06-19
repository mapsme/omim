package com.mapswithme.maps.bookmarks;

import android.app.DownloadManager;
import android.content.Context;
import android.net.Uri;
import android.os.Environment;
import android.support.annotation.NonNull;
import android.text.TextUtils;
import android.util.Pair;

import com.mapswithme.maps.MwmApplication;
import com.mapswithme.maps.bookmarks.data.BookmarkManager;
import com.mapswithme.maps.bookmarks.persistence.BookmarkCategoryArchive;

public class BookmarksDownloadManager
{
  private static final String QUERY_PARAM_ID_KEY = "id";
  private static final String QUERY_PARAM_NAME_KEY = "name";

  @NonNull
  private final Context mContext;

  private BookmarksDownloadManager(@NonNull Context context)
  {
    mContext = context.getApplicationContext();
  }

  @NonNull
  public String enqueueRequestBlocking(@NonNull String url)
  {
    DownloadManager downloadManager =
        (DownloadManager) mContext.getSystemService(Context.DOWNLOAD_SERVICE);
    if (downloadManager == null)
    {
      throw new NullPointerException(
          "Download manager is null, failed to download url = " + url);
    }

    Pair<Uri, Uri> uriPair = prepareUriPair(url);
    Uri srcUri = uriPair.first;
    Uri dstUri = uriPair.second;

    String title = makeTitle(srcUri);
    String serverId = dstUri.getLastPathSegment();

    DownloadManager.Request request = makeRequest(dstUri, title, serverId);
    long contentProviderId = downloadManager.enqueue(request);
    BookmarkCategoryArchive archive = putArchiveToDb(contentProviderId, serverId);
    return archive.getServerId();
  }

  private DownloadManager.Request makeRequest(@NonNull Uri dstUri, @NonNull String title,
                                              @NonNull String serverId)
  {
    return new DownloadManager
          .Request(dstUri)
          .setNotificationVisibility(DownloadManager.Request.VISIBILITY_VISIBLE)
          .setTitle(title)
          .setDestinationInExternalFilesDir(mContext, Environment.DIRECTORY_PODCASTS, serverId);
  }

  @NonNull
  private BookmarkCategoryArchive putArchiveToDb(long contentProviderId, @NonNull String serverId)
  {
    BookmarkCategoryArchive archive = new BookmarkCategoryArchive(contentProviderId, serverId);
    getApp().getAppDb().bookmarkArchiveDao().createOrUpdate(archive);
    return archive;
  }

  @NonNull
  private MwmApplication getApp()
  {
    return (MwmApplication)mContext.getApplicationContext();
  }

  @NonNull
  private static String makeTitle(@NonNull Uri srcUri)
  {
    String title = srcUri.getQueryParameter(QUERY_PARAM_NAME_KEY);
    return TextUtils.isEmpty(title) ? srcUri.getQueryParameter(QUERY_PARAM_ID_KEY) : title;
  }

  @NonNull
  private static Pair<Uri, Uri> prepareUriPair(@NonNull String url)
  {
    Uri srcUri = Uri.parse(url);
    String fileId = srcUri.getQueryParameter(QUERY_PARAM_ID_KEY);
    if (TextUtils.isEmpty(fileId))
      throw new IllegalArgumentException("File id not found");

    String downloadUrl = BookmarkManager.INSTANCE.getCatalogDownloadUrl(fileId);
    Uri.Builder builder = Uri.parse(downloadUrl).buildUpon();

    for (String each : srcUri.getQueryParameterNames())
    {
      builder.appendQueryParameter(each, srcUri.getQueryParameter(each));
    }
    Uri dstUri = builder.build();
    return new Pair<>(srcUri, dstUri);
  }

  @NonNull
  public static BookmarksDownloadManager from(@NonNull Context context)
  {
    return new BookmarksDownloadManager(context);
  }
}
