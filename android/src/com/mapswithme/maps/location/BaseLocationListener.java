package com.mapswithme.maps.location;

import android.location.Location;
import android.location.LocationListener;
import android.os.Bundle;
import android.support.annotation.NonNull;

import com.mapswithme.util.log.Logger;
import com.mapswithme.util.log.LoggerFactory;

class BaseLocationListener implements LocationListener, com.google.android.gms.location.LocationListener
{
  private static final String TAG = BaseLocationListener.class.getSimpleName();
  private static final Logger LOGGER = LoggerFactory.INSTANCE.getLogger(LoggerFactory.Type.LOCATION);
  @NonNull
  private final LocationFixChecker mLocationFixChecker;

  BaseLocationListener(@NonNull LocationFixChecker locationFixChecker)
  {
    mLocationFixChecker = locationFixChecker;
  }

  @Override
  public void onLocationChanged(Location location)
  {
    // Completely ignore locations without lat and lon
    if (location.getAccuracy() <= 0.0)
      return;

    if (mLocationFixChecker.isLocationBetterThanLast(location))
    {
      LocationHelper.INSTANCE.resetMagneticField(location);
      LocationHelper.INSTANCE.onLocationUpdated(location);
      LocationHelper.INSTANCE.notifyLocationUpdated();
    }
    else
    {
      Location last = LocationHelper.INSTANCE.getSavedLocation();
      if (last != null)
      {
        LOGGER.d(TAG, "The new location from '" + location.getProvider()
                 + "' is worse than the last one from '" + last.getProvider() + "'");
      }
    }
  }

  @Override
  public void onProviderDisabled(String provider)
  {
    LOGGER.d(TAG, "Disabled location provider: " + provider);
  }

  @Override
  public void onProviderEnabled(String provider)
  {
    LOGGER.d(TAG, "Enabled location provider: " + provider);
  }

  @Override
  public void onStatusChanged(String provider, int status, Bundle extras)
  {
    LOGGER.d(TAG, "Status changed for location provider: " + provider + "; new status = " + status);
  }
}
