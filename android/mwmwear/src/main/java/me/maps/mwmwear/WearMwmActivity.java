package me.maps.mwmwear;

import android.app.Activity;
import android.content.IntentSender;
import android.location.Location;
import android.os.Bundle;
import android.support.wearable.view.WatchViewStub;
import android.util.Log;
import android.widget.TextView;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GooglePlayServicesUtil;
import com.google.android.gms.common.api.GoogleApiClient;
import com.google.android.gms.common.api.ResultCallback;
import com.google.android.gms.common.api.Status;
import com.google.android.gms.location.LocationListener;
import com.google.android.gms.location.LocationRequest;
import com.google.android.gms.location.LocationServices;
import com.google.android.gms.wearable.Wearable;

public class WearMwmActivity extends Activity implements LocationListener,
    GoogleApiClient.ConnectionCallbacks, GoogleApiClient.OnConnectionFailedListener
{
  private static final String TAG = "WEAR";
  private static final int REQUEST_RESOLVE = 1000;
  private static final long LOCATION_UPDATE_INTERVAL = 500;

  private GoogleApiClient mGmsClient;
  private TextView mTextView;

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
        mTextView = (TextView) stub.findViewById(R.id.text);
        mTextView.setText("Hello world!");
      }
    });
  }

  @Override
  protected void onResume()
  {
    super.onResume();
    connectGms();
  }

  @Override
  protected void onPause()
  {
    super.onPause();

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
              Log.e(TAG,
                  "Failed in requesting location updates, "
                      + "status code: "
                      + status.getStatusCode()
                      + ", message: "
                      + status.getStatusMessage());
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
    Log.d(TAG, "onLocationChanged: " + location);
    // TODO request new bitmap from mobile
  }
}
