package com.mapswithme.maps.bookmarks.persistence;

import android.app.IntentService;
import android.content.Context;
import android.content.Intent;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import com.mapswithme.maps.bookmarks.data.Error;

public class SystemDownloadCompletedService extends IntentService
{
  public static final String EXTRA_REQUEST = "request";

  public SystemDownloadCompletedService()
  {
    super("SystemDownloadCompletedService");
  }

  @Override
  protected void onHandleIntent(@Nullable Intent intent)
  {
    if (intent == null)
      return;

    CatalogCategoryRequest<?, Error> request = intent.getParcelableExtra(EXTRA_REQUEST);
    if (request == null)
      throw new IllegalArgumentException("Request not found in extras");

    request.doInBackground(this, intent);
  }

  public static <T> Intent makeIntent(@NonNull Context context,
                                      @NonNull CatalogCategoryRequest<T, Error> request)
  {
    Intent intent = new Intent(context, SystemDownloadCompletedService.class);
    return makeIntent(intent, request);
  }

  public static <T> Intent makeIntent(@NonNull Intent src,
                                      @NonNull CatalogCategoryRequest<T, Error> request)
  {
    return src.putExtra(EXTRA_REQUEST, request);
  }
}
