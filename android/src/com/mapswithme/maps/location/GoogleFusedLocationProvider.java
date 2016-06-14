package com.mapswithme.maps.location;

import android.app.Activity;
import android.content.IntentSender;
import android.location.Location;
import android.os.Bundle;
import android.support.annotation.NonNull;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.api.GoogleApiClient;
import com.google.android.gms.common.api.PendingResult;
import com.google.android.gms.common.api.ResultCallback;
import com.google.android.gms.common.api.Status;
import com.google.android.gms.location.LocationListener;
import com.google.android.gms.location.LocationRequest;
import com.google.android.gms.location.LocationServices;
import com.google.android.gms.location.LocationSettingsRequest;
import com.google.android.gms.location.LocationSettingsResult;
import com.google.android.gms.location.LocationSettingsStatusCodes;
import com.mapswithme.maps.MwmApplication;

class GoogleFusedLocationProvider extends BaseLocationProvider
                               implements GoogleApiClient.ConnectionCallbacks,
                                          GoogleApiClient.OnConnectionFailedListener,
                                          LocationListener
{
  private static final String GMS_LOCATION_PROVIDER = "fused";

  private final GoogleApiClient mGoogleApiClient;
  private LocationRequest mLocationRequest;
  private PendingResult<LocationSettingsResult> mLocationSettingsResult;

  GoogleFusedLocationProvider()
  {
    mGoogleApiClient = new GoogleApiClient.Builder(MwmApplication.get())
                                          .addApi(LocationServices.API)
                                          .addConnectionCallbacks(this)
                                          .addOnConnectionFailedListener(this)
                                          .build();
  }

  @Override
  protected boolean start()
  {
    if (mGoogleApiClient == null || mGoogleApiClient.isConnected() || mGoogleApiClient.isConnecting())
      return true;

    mLocationRequest = LocationRequest.create();
//    mLocationRequest.setPriority(LocationHelper.INSTANCE.isHighAccuracy() ? LocationRequest.PRIORITY_HIGH_ACCURACY
//                                                                          : LocationRequest.PRIORITY_BALANCED_POWER_ACCURACY);
    // TODO @yunikkk
    // Currently there are some problems concerning location strategies switching.
    // With LocationRequest.PRIORITY_BALANCED_POWER_ACCURACY priority GPS is not used and location icon isn't shown in system navbar,
    // hence it confuses user.
    // We should reconsider if balanced mode is needed at all after results of tests for battery usage will arrive.
    mLocationRequest.setPriority(LocationRequest.PRIORITY_HIGH_ACCURACY);
    long interval = LocationHelper.INSTANCE.getInterval();
    mLocationRequest.setInterval(interval);
    mLocationRequest.setFastestInterval(interval / 2);

    mGoogleApiClient.connect();
    return true;
  }

  @Override
  protected void stop()
  {
    if (mGoogleApiClient == null)
      return;

    if (mGoogleApiClient.isConnected())
      LocationServices.FusedLocationApi.removeLocationUpdates(mGoogleApiClient, this);

    if (mLocationSettingsResult != null && !mLocationSettingsResult.isCanceled())
      mLocationSettingsResult.cancel();

    mGoogleApiClient.disconnect();
  }

  @Override
  protected boolean isLocationBetterThanLast(Location newLocation, Location lastLocation)
  {
    // We believe that google services always returns good locations.
    return (isFromFusedProvider(newLocation) ||
            (!isFromFusedProvider(lastLocation) && super.isLocationBetterThanLast(newLocation, lastLocation)));

  }

  private static boolean isFromFusedProvider(Location location)
  {
    return GMS_LOCATION_PROVIDER.equalsIgnoreCase(location.getProvider());
  }

  @Override
  public void onConnected(Bundle bundle)
  {
    sLogger.d("Fused onConnected. Bundle " + bundle);
    checkSettingsAndRequestUpdates();
  }

  private void checkSettingsAndRequestUpdates()
  {
    LocationSettingsRequest.Builder builder = new LocationSettingsRequest.Builder().addLocationRequest(mLocationRequest);
    builder.setAlwaysShow(true); // hides 'never' button in resolve dialog afterwards.
    mLocationSettingsResult = LocationServices.SettingsApi.checkLocationSettings(mGoogleApiClient, builder.build());
    mLocationSettingsResult.setResultCallback(new ResultCallback<LocationSettingsResult>()
    {
      @Override
      public void onResult(@NonNull LocationSettingsResult locationSettingsResult)
      {
        final Status status = locationSettingsResult.getStatus();
        switch (status.getStatusCode())
        {
        case LocationSettingsStatusCodes.SUCCESS:
          break;

        case LocationSettingsStatusCodes.RESOLUTION_REQUIRED:
          // Location settings are not satisfied, but this can be fixed by showing the user a dialog.
          resolveError(status);
          return;

        case LocationSettingsStatusCodes.SETTINGS_CHANGE_UNAVAILABLE:
          // Location settings are not satisfied. However, we have no way to fix the settings so we won't show the dialog.
          break;
        }

        requestLocationUpdates();
      }
    });
  }

  private static void resolveError(Status status)
  {
    if (LocationHelper.INSTANCE.isLocationStopped())
      return;

    LocationHelper.INSTANCE.stop(false);
    Activity activity = MwmApplication.backgroundTracker().getTopActivity();
    if (activity != null)
    {
      try
      {
        status.startResolutionForResult(activity, LocationHelper.REQUEST_RESOLVE_ERROR);
      } catch (IntentSender.SendIntentException ignored) {}
    }
  }

  private void requestLocationUpdates()
  {
    if (!mGoogleApiClient.isConnected())
      return;

    LocationServices.FusedLocationApi.requestLocationUpdates(mGoogleApiClient, mLocationRequest, this);
    LocationHelper.INSTANCE.startSensors();
    Location last = LocationServices.FusedLocationApi.getLastLocation(mGoogleApiClient);
    if (last != null)
      onLocationChanged(last);
  }

  @Override
  public void onConnectionSuspended(int i)
  {
    sLogger.d("Fused onConnectionSuspended. Code " + i);
  }

  @Override
  public void onConnectionFailed(@NonNull ConnectionResult connectionResult)
  {
    sLogger.d("Fused onConnectionFailed. Fall back to native provider. ConnResult " + connectionResult);
    // TODO handle error in a smarter way
    LocationHelper.INSTANCE.initProvider(true);
  }
}
