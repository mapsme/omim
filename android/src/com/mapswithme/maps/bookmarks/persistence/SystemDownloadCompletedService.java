package com.mapswithme.maps.bookmarks.persistence;

import android.app.IntentService;
import android.content.Context;
import android.content.Intent;
import android.support.annotation.Nullable;

import com.mapswithme.maps.bookmarks.data.Error;

public class SystemDownloadCompletedService extends IntentService
{
  public static final String EXTRA_REQUEST = "operation_factory";

  public SystemDownloadCompletedService()
  {
    super("GetFileMetaDataService");
  }

  @Override
  protected void onHandleIntent(@Nullable Intent intent)
  {
    if (intent == null)
      return;

    CatalogCategoryRequest<?, Error> request = intent.getParcelableExtra(EXTRA_REQUEST);
    if (request == null)
      throw new IllegalArgumentException("Factory not found in extras");

    request.doInBackground(this, intent);
  }

  public static <T> Intent makeIntent(Context context, CatalogCategoryRequest<T, Error> request)
  {
    Intent intent = new Intent(context, SystemDownloadCompletedService.class);
    return makeIntent(intent, request);
  }

  public static <T> Intent makeIntent(Intent src, CatalogCategoryRequest<T, Error> request)
  {
    return src.putExtra(EXTRA_REQUEST, request);
  }
}
