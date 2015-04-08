package me.maps.mwmwear;

import android.app.Activity;
import android.content.IntentSender;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.location.Location;
import android.net.Uri;
import android.os.Bundle;
import android.support.wearable.view.GridPagerAdapter;
import android.support.wearable.view.GridViewPager;
import android.support.wearable.view.WatchViewStub;
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
import java.util.List;
import java.util.concurrent.TimeUnit;

public class WearMwmActivity extends Activity implements LocationListener,
    GoogleApiClient.ConnectionCallbacks, GoogleApiClient.OnConnectionFailedListener, DataApi.DataListener
{
  private static final String TAG = "Wear";
  private static final int REQUEST_RESOLVE = 1000;
  private static final long LOCATION_UPDATE_INTERVAL = 500;
  private static final String PATH_IMAGE = "/image";
  private static final String KEY_IMAGE = "Image";
  private static final String KEY_SEARCH_RESULT = "SearchResult";

  private String mNodeId;
  private GoogleApiClient mGmsClient;

  private GridViewPager mGrid;
  private MainWearAdapter mAdapter;

  @Override
  protected void onCreate(Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.activity_main);
    final WatchViewStub stub = (WatchViewStub) findViewById(R.id.watch_view_stub);
    stub.setRectLayout(R.layout.rect_activity_main);
    stub.setRoundLayout(R.layout.round_activity_main);
    stub.setOnLayoutInflatedListener(new WatchViewStub.OnLayoutInflatedListener()
    {
      @Override
      public void onLayoutInflated(WatchViewStub stub)
      {
        mGrid = (GridViewPager) stub.findViewById(R.id.vp__main);
        mGrid.setAdapter(getAdapter());
        mGrid.setOnPageChangeListener(new GridViewPager.OnPageChangeListener() {
          @Override
          public void onPageScrolled(int i, int i1, float v, float v1, int i2, int i3)
          {

          }

          @Override
          public void onPageSelected(int row, int column)
          {
            Log.d(TAG, "Page selected. row : " + row + ", column : " + column);
          }

          @Override
          public void onPageScrollStateChanged(int i)
          {

          }
        });
      }
    });
  }

  private GridPagerAdapter getAdapter()
  {
    if (mAdapter == null)
      mAdapter = new MainWearAdapter(this);

    return mAdapter;
  }

  @Override
  protected void onResume()
  {
    super.onResume();
    connectGms();
    initNodeId();
  }

  @Override
  protected void onPause()
  {
    super.onPause();
    mGmsClient.disconnect();
  }

  private void connectGms()
  {
    mGmsClient = new GoogleApiClient.Builder(this)
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
    Wearable.DataApi.deleteDataItems(mGmsClient, Uri.parse("wear://" + mNodeId + PATH_IMAGE));
    Wearable.DataApi.addListener(mGmsClient, this);
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
        result.startResolutionForResult(WearMwmActivity.this, REQUEST_RESOLVE);
      } catch (IntentSender.SendIntentException e)
      {
        e.printStackTrace();
      }
    else
      GooglePlayServicesUtil.getErrorDialog(result.getErrorCode(), WearMwmActivity.this, REQUEST_RESOLVE).show();
  }

  @Override
  public void onLocationChanged(Location location)
  {
    Log.d(TAG, "onLocationChanged: " + location + ", grid item : " + mGrid.getCurrentItem());
    if (mGrid.getCurrentItem().y == MainWearAdapter.ARROW_FRAGMENT)
      requestMapUpdate();
  }

  private void requestMapUpdate()
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
      Log.d(TAG, "Data changed. Event type: " + event.getType() + ", event : " + event.toString());
      if (event.getType() == DataEvent.TYPE_CHANGED)
      {
        // DataItem changed
        DataItem item = event.getDataItem();
        final String path = item.getUri().getPath();
        if (path.equals(PATH_IMAGE))
        {
          DataMap dataMap = DataMapItem.fromDataItem(item).getDataMap();
          Asset asset = dataMap.getAsset(KEY_IMAGE);
          if (asset == null)
            throw new IllegalArgumentException("Image asset must be non-null.");

          final ConnectionResult result = mGmsClient.blockingConnect(2000, TimeUnit.MILLISECONDS);
          if (!result.isSuccess())
          {
            Log.e(TAG, "Unsuccessful PATH_IMAGE result retrieved!");
            return;
          }

          setMapBgFromAsset(asset);
        }
      }
    }
  }

  private void setMapBgFromAsset(final Asset asset)
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

        mGrid.post(new Runnable()
        {
          @Override
          public void run()
          {
            mAdapter.getMapFragment().setBitmap(bitmap);
          }
        });
      }
    }).start();
  }
}
