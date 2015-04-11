package me.maps.mwmwear.communication;

import android.content.IntentSender;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.location.Location;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GooglePlayServicesUtil;
import com.google.android.gms.common.api.GoogleApiClient;
import com.google.android.gms.common.api.ResultCallback;
import com.google.android.gms.common.api.Status;
import com.google.android.gms.location.LocationListener;
import com.google.android.gms.location.LocationRequest;
import com.google.android.gms.location.LocationServices;
import com.google.android.gms.wearable.Asset;
import com.google.android.gms.wearable.DataApi;
import com.google.android.gms.wearable.DataEvent;
import com.google.android.gms.wearable.DataEventBuffer;
import com.google.android.gms.wearable.DataItem;
import com.google.android.gms.wearable.DataMap;
import com.google.android.gms.wearable.DataMapItem;
import com.google.android.gms.wearable.MessageApi;
import com.google.android.gms.wearable.Node;
import com.google.android.gms.wearable.Wearable;

import java.io.InputStream;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

import me.maps.mwmwear.WearMwmActivity;
import me.maps.mwmwear.fragment.SearchAdapter;

public class WearableManager implements LocationListener, GoogleApiClient.ConnectionCallbacks, GoogleApiClient.OnConnectionFailedListener, DataApi.DataListener
{
  private static final String TAG = "Wear";
  private static final int REQUEST_RESOLVE = 1000;
  private static final long LOCATION_UPDATE_INTERVAL = 500;
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

  private final WeakReference<WearMwmActivity> mActivity;
  private WeakReference<SearchListener> mListener;

  private String mNodeId;
  private static GoogleApiClient mGmsClient;

  public WearableManager(WearMwmActivity activity)
  {
    mActivity = new WeakReference<>(activity);
  }

  public void startCommunication()
  {
    connectGms();
    initNodeId();
  }

  public void endCommunication()
  {
    Wearable.DataApi.removeListener(mGmsClient, this);
    mGmsClient.disconnect();
  }

  public void makeSearchCategoryRequest(String category, SearchListener listener)
  {
    Log.d("TEST", "Make search request");
    mListener = new WeakReference<>(listener);
    Wearable.MessageApi.
        sendMessage(mGmsClient, mNodeId, PATH_SEARCH_CATEGORY, category.getBytes()).
        setResultCallback(new ResultCallback<MessageApi.SendMessageResult>()
        {
          @Override
          public void onResult(MessageApi.SendMessageResult sendMessageResult)
          {
            final Status status = sendMessageResult.getStatus();
            Log.d(TAG, "Send search message with status code: " + status.getStatusCode());
          }
        });
  }

  private void connectGms()
  {
    if (mGmsClient != null)
      return;
    mGmsClient = new GoogleApiClient.Builder(mActivity.get())
        .addApi(LocationServices.API)
        .addApi(Wearable.API)
        .addConnectionCallbacks(this)
        .addOnConnectionFailedListener(this)
        .build();

    mGmsClient.connect();
  }

  @Override
  public void onConnected(Bundle bundle)
  {
    Log.d(TAG, "onConnected: " + bundle);
    makeLocationRequest();
    cleanDataItems();
    Wearable.DataApi.addListener(mGmsClient, this);
  }

  private void cleanDataItems()
  {
    Wearable.DataApi.deleteDataItems(mGmsClient, Uri.parse("wear://" + mNodeId + PATH_IMAGE));
    Wearable.DataApi.deleteDataItems(mGmsClient, Uri.parse("wear://" + mNodeId + PATH_SEARCH_CATEGORY));
    Wearable.DataApi.deleteDataItems(mGmsClient, Uri.parse("wear://" + mNodeId + PATH_SEARCH_QUERY));
  }

  private void makeLocationRequest()
  {
    LocationRequest locationRequest = LocationRequest.create()
        .setPriority(LocationRequest.PRIORITY_HIGH_ACCURACY)
        .setInterval(LOCATION_UPDATE_INTERVAL)
        .setFastestInterval(LOCATION_UPDATE_INTERVAL / 2);

    LocationServices.FusedLocationApi
        .requestLocationUpdates(mGmsClient, locationRequest, this)
        .setResultCallback(new ResultCallback<Status>()
        {
          @Override
          public void onResult(Status status)
          {
            if (status.isSuccess())
              Log.d(TAG, "Successfully requested location updates");
            else
              Log.e(TAG, "Failed in requesting location updates, status: " + status.toString());
          }
        });
  }

  @Override
  public void onConnectionSuspended(int i)
  {
    Log.d(TAG, "onConnectionSuspended: " + i);
  }

  @Override
  public void onConnectionFailed(ConnectionResult result)
  {
    Log.d(TAG, "onConnectionFailed: " + result);
    if (result.hasResolution())
      try
      {
        result.startResolutionForResult(mActivity.get(), REQUEST_RESOLVE);
      } catch (IntentSender.SendIntentException e)
      {
        e.printStackTrace();
      }
    else
      GooglePlayServicesUtil.getErrorDialog(result.getErrorCode(), mActivity.get(), REQUEST_RESOLVE).show();
  }

  @Override
  public void onLocationChanged(Location location)
  {
    requestMapUpdate();
    if (mListener != null)
      mListener.get().onLocationChanged(location);
  }

  public void requestMapUpdate()
  {
    Wearable.MessageApi.
        sendMessage(mGmsClient, mNodeId, PATH_IMAGE, new byte[0]).
        setResultCallback(new ResultCallback<MessageApi.SendMessageResult>()
        {
          @Override
          public void onResult(MessageApi.SendMessageResult sendMessageResult)
          {
            final Status status = sendMessageResult.getStatus();
            Log.d(TAG, "Send message with status code: " + status.getStatusCode());
          }
        });
  }

  public static Location getLastLocation()
  {
    return LocationServices.FusedLocationApi.getLastLocation(mGmsClient);
  }

  private void initNodeId()
  {
    new Thread(new Runnable()
    {
      @Override
      public void run()
      {
        List<Node> nodes = Wearable.NodeApi.getConnectedNodes(mGmsClient).await().getNodes();
        mNodeId = (nodes == null || nodes.size() == 0) ? "" : nodes.get(0).getId();
      }
    }).start();
  }

  @Override
  public void onDataChanged(DataEventBuffer dataEventBuffer)
  {
    for (DataEvent event : dataEventBuffer)
    {
      if (event.getType() == DataEvent.TYPE_CHANGED)
      {
        // DataItem changed
        final DataItem item = event.getDataItem();
        final DataMap dataMap = DataMapItem.fromDataItem(item).getDataMap();
        final String path = item.getUri().getPath();
        Log.d(TAG, "Data changed, path : " + path);
        if (path.equals(PATH_IMAGE))
        {
          Asset asset = dataMap.getAsset(KEY_IMAGE);
          if (asset == null)
            throw new IllegalArgumentException("Image asset must be non-null.");

          final ConnectionResult result = mGmsClient.blockingConnect(2000, TimeUnit.MILLISECONDS);
          if (!result.isSuccess())
          {
            Log.e(TAG, "Unsuccessful PATH_IMAGE result retrieved!");
            return;
          }

          processBitmapAsset(asset);
        }
        else if (path.equals(PATH_SEARCH_CATEGORY))
        {
          List<DataMap> items = dataMap.getDataMapArrayList(KEY_SEARCH_RESULT);
          List<SearchAdapter.SearchResult> searchResults = new ArrayList<>();
          for (DataMap dataListItem : items)
          {
            searchResults.add(new SearchAdapter.SearchResult(dataListItem.getString(KEY_SEARCH_NAME),
                dataListItem.getString(KEY_SEARCH_AMENITY),
                dataListItem.getDouble(KEY_SEARCH_LAT),
                dataListItem.getDouble(KEY_SEARCH_LON)));
          }

          if (mListener != null)
            mListener.get().onCategorySearchComplete("", searchResults);
        } else if (path.equals(PATH_SEARCH_QUERY))
        {
          // TODO process voice search query
        }

        cleanDataItems();
      }
    }
  }

  private void processBitmapAsset(final Asset asset)
  {
    new Thread(new Runnable()
    {
      @Override
      public void run()
      {
        final InputStream assetStream = Wearable.DataApi.getFdForAsset(mGmsClient, asset).
            await().getInputStream();

        if (assetStream == null)
        {
          Log.d(TAG, "Cannot open asset stream.");
          return;
        }

        final Bitmap bitmap = BitmapFactory.decodeStream(assetStream);
        if (bitmap == null)
        {
          Log.d(TAG, "Decoded bitmap == null");
          return;
        }

        mActivity.get().setMapBitmap(bitmap);
      }
    }).start();
  }
}
