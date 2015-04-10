package com.mapswithme.wearable;

import android.app.Activity;
import android.graphics.Bitmap;
import android.location.Location;
import android.os.Bundle;
import android.util.Log;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.api.GoogleApiClient;
import com.google.android.gms.wearable.Asset;
import com.google.android.gms.wearable.DataApi;
import com.google.android.gms.wearable.DataEventBuffer;
import com.google.android.gms.wearable.DataMap;
import com.google.android.gms.wearable.MessageApi;
import com.google.android.gms.wearable.MessageEvent;
import com.google.android.gms.wearable.PutDataMapRequest;
import com.google.android.gms.wearable.Wearable;
import com.mapswithme.maps.MapsMeService;
import com.mapswithme.maps.location.LocationHelper;

import java.io.ByteArrayOutputStream;
import java.lang.ref.WeakReference;
import java.util.ArrayList;

public class WearableManager implements MessageApi.MessageListener, DataApi.DataListener, GoogleApiClient.ConnectionCallbacks, GoogleApiClient.OnConnectionFailedListener, MapsMeService.RenderService.Listener, MapsMeService.LookupService.Listener
{
  private static final String PATH_IMAGE = "/image";
  private static final String KEY_IMAGE = "Image";
  private static final String PATH_SEARCH_CATEGORY = "/search/category";
  private static final String PATH_SEARCH_QUERY = "/search/request";
  private static final String KEY_SEARCH_QUERY = "Query";
  private static final String KEY_SEARCH_NAME = "Name";
  private static final String KEY_SEARCH_AMENITY = "Amenity";
  private static final String KEY_SEARCH_DISTANCE = "Distance";
  private static final String KEY_SEARCH_LAT = "Lat";
  private static final String KEY_SEARCH_LON = "Lon";
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
    MapsMeService.RenderService.removeListener(this);
    MapsMeService.LookupService.removeListener(this);
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
    MapsMeService.RenderService.addListener(this);
    MapsMeService.LookupService.addListener(this);
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
    final String path = messageEvent.getPath();
    Log.d(TAG, "Message received, path : " + path);
    if (path.equals(PATH_IMAGE))
      requestMapRender(LocationHelper.INSTANCE.getLastLocation());
    else if (path.equals(PATH_SEARCH_CATEGORY))
    {
      // TODO get rid of hardcoded params
      final String category = new String(messageEvent.getData());
      MapsMeService.LookupService.lookup(LocationHelper.INSTANCE.getLastLocation().getLatitude(),
          LocationHelper.INSTANCE.getLastLocation().getLongitude(),
          25000, category, "en", 20);
    }
  }

  public void requestMapRender(Location location)
  {
    Log.d("Wear", "Request map render");
    // TODO pass correct params from wearable's sensors
    MapsMeService.RenderService.renderMap(location.getLatitude(), location.getLongitude(),
        15, 320, 320, 180);
  }

  private Asset getBitmapAsset(Bitmap bitmap)
  {
    Log.d(TAG, "Compress bitmap to PNG asset size : " + bitmap.getByteCount());
    final ByteArrayOutputStream stream = new ByteArrayOutputStream();
    bitmap.compress(Bitmap.CompressFormat.PNG, 100, stream);
    Log.d(TAG, "Compressed PNG byte array :  " + stream.toByteArray().length);
    return Asset.createFromBytes(stream.toByteArray());
  }

  @Override
  public void onRenderComplete(Bitmap bitmap)
  {
    final PutDataMapRequest mapRequest = PutDataMapRequest.create(PATH_IMAGE);
    final Asset asset = getBitmapAsset(bitmap);
    Log.d(TAG, "Asset size : " + asset.toString());
    mapRequest.getDataMap().putAsset(KEY_IMAGE, asset);
    Wearable.DataApi.putDataItem(mGmsClient, mapRequest.asPutDataRequest());
  }

  @Override
  public void onLookupComplete(MapsMeService.LookupService.LookupItem[] items)
  {
    Log.d(TAG, "Lookup complete, items size : " + items.length);
    final PutDataMapRequest mapRequest = PutDataMapRequest.create(PATH_SEARCH_CATEGORY);
    ArrayList<DataMap> dataList = new ArrayList<>();
    for (MapsMeService.LookupService.LookupItem lookupItem : items)
    {

      DataMap dataItem = new DataMap();
      dataItem.putString(KEY_SEARCH_NAME, lookupItem.mName);
      dataItem.putString(KEY_SEARCH_AMENITY, lookupItem.mAmenity);
      dataItem.putDouble(KEY_SEARCH_LAT, lookupItem.mLatitude);
      dataItem.putDouble(KEY_SEARCH_LON, lookupItem.mLongitude);
      dataList.add(dataItem);
    }
    mapRequest.getDataMap().putDataMapArrayList(KEY_SEARCH_RESULT, dataList);
    Log.d(TAG, "Reply to wearable with search results, size : " + dataList.size());
    Wearable.DataApi.putDataItem(mGmsClient, mapRequest.asPutDataRequest());
  }
}
