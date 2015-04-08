package com.mapswithme.wearable;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.Bundle;
import android.util.Log;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.api.GoogleApiClient;
import com.google.android.gms.wearable.Asset;
import com.google.android.gms.wearable.DataApi;
import com.google.android.gms.wearable.DataEventBuffer;
import com.google.android.gms.wearable.MessageApi;
import com.google.android.gms.wearable.MessageEvent;
import com.google.android.gms.wearable.PutDataMapRequest;
import com.google.android.gms.wearable.Wearable;
import com.mapswithme.maps.R;

import java.io.ByteArrayOutputStream;
import java.lang.ref.WeakReference;

public class WearableManager implements MessageApi.MessageListener, DataApi.DataListener, GoogleApiClient.ConnectionCallbacks, GoogleApiClient.OnConnectionFailedListener
{
  private static final String PATH_IMAGE = "/image";
  private static final String KEY_IMAGE = "Image";
  private static final String KEY_SEARCH_RESULT = "SearchResult";
  private static final String TAG = "Wear";

  private GoogleApiClient mGmsClient;
  private WeakReference<Activity> mActivity;

  public WearableManager(Activity activity)
  {
    mActivity = new WeakReference<>(activity);
  }

  public void startCommunication()
  {
    mGmsClient = new GoogleApiClient.Builder(mActivity.get())
        .addApi(Wearable.API)
        .addConnectionCallbacks(this)
        .addOnConnectionFailedListener(this)
        .build();
    mGmsClient.connect();
  }


  public void endCommunication()
  {
    mGmsClient.disconnect();
    Wearable.MessageApi.removeListener(mGmsClient, this);
  }

  @Override
  public void onDataChanged(DataEventBuffer dataEventBuffer)
  {
    Log.d(TAG, "Data changed");
  }

  @Override
  public void onConnected(Bundle bundle)
  {
    Log.d(TAG, "WM Connected");
    Wearable.MessageApi.addListener(mGmsClient, this);
  }

  @Override
  public void onConnectionSuspended(int i)
  {
    Log.d(TAG, "WM connection suspended");
  }

  @Override
  public void onConnectionFailed(ConnectionResult connectionResult)
  {
    Log.d(TAG, "WM connection failed: " + connectionResult);
  }

  @Override
  public void onMessageReceived(MessageEvent messageEvent)
  {
    Log.d(TAG, "Message received");
    if (messageEvent.getPath().equals(PATH_IMAGE))
    {
      final PutDataMapRequest mapRequest = PutDataMapRequest.create(PATH_IMAGE);
      mapRequest.getDataMap().putAsset(KEY_IMAGE, getBitmapAsset());
      Wearable.DataApi.putDataItem(mGmsClient, mapRequest.asPutDataRequest());
    }
  }

  private Asset getBitmapAsset()
  {
    // TODO pass correct drawable from core
    final Bitmap bitmap = BitmapFactory.decodeResource(mActivity.get().getResources(), R.drawable.ic_direction);
    final ByteArrayOutputStream stream = new ByteArrayOutputStream();
    bitmap.compress(Bitmap.CompressFormat.PNG, 100, stream);
    return Asset.createFromBytes(stream.toByteArray());
  }
}
