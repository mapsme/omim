package com.mapswithme.util;

import android.annotation.SuppressLint;
import android.content.ContentResolver;
import android.content.Context;
import android.location.Location;
import android.location.LocationManager;
import android.os.Build;
import android.os.SystemClock;
import android.provider.Settings;
import android.text.TextUtils;
import android.view.Surface;

import com.mapswithme.maps.MwmApplication;
import com.mapswithme.maps.location.LocationHelper;
import com.mapswithme.util.log.Logger;
import com.mapswithme.util.log.LoggerFactory;

import java.util.List;

public class LocationUtils
{
  private LocationUtils() {}

  public static final long LOCATION_EXPIRATION_TIME_MILLIS_SHORT = 60 * 1000; // 1 minute
  public static final long LOCATION_EXPIRATION_TIME_MILLIS_LONG = 6 * 60 * 60 * 1000; // 6 hours
  private static final Logger LOGGER = LoggerFactory.INSTANCE.getLogger(LoggerFactory.Type.LOCATION);
  private static final String TAG = LocationUtils.class.getSimpleName();

  /**
   * Correct compass angles due to display orientation.
   */
  public static double correctCompassAngle(int displayOrientation, double angle)
  {
    double correction = 0;
    switch (displayOrientation)
    {
    case Surface.ROTATION_90:
      correction = Math.PI / 2.0;
      break;
    case Surface.ROTATION_180:
      correction = Math.PI;
      break;
    case Surface.ROTATION_270:
      correction = (3.0 * Math.PI / 2.0);
      break;
    }

    // negative values (like -1.0) should remain negative (indicates that no direction available)
    if (angle >= 0.0)
      angle = correctAngle(angle, correction);

    return angle;
  }

  public static double correctAngle(double angle, double correction)
  {
    double res = angle + correction;

    final double twoPI = 2.0 * Math.PI;
    res %= twoPI;

    // normalize angle into [0, 2PI]
    if (res < 0.0)
      res += twoPI;

    return res;
  }

  public static boolean isExpired(Location l, long millis, long expirationMillis)
  {
    long timeDiff;
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1)
      timeDiff = (SystemClock.elapsedRealtimeNanos() - l.getElapsedRealtimeNanos()) / 1000000;
    else
      timeDiff = System.currentTimeMillis() - millis;
    return (timeDiff > expirationMillis);
  }

  public static double getDiff(Location lastLocation, Location newLocation)
  {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1)
      return (newLocation.getElapsedRealtimeNanos() - lastLocation.getElapsedRealtimeNanos()) * 1.0E-9;
    else
    {
      long time = newLocation.getTime();
      long lastTime = lastLocation.getTime();
      if (!isSameLocationProvider(newLocation.getProvider(), lastLocation.getProvider()))
      {
        // Do compare current and previous system times in case when
        // we have incorrect time settings on a device.
        time = System.currentTimeMillis();
        lastTime = LocationHelper.INSTANCE.getSavedLocationTime();
      }

      return (time - lastTime) * 1.0E-3;
    }
  }

  private static boolean isSameLocationProvider(String p1, String p2)
  {
    return (p1 != null && p1.equals(p2));
  }

  @SuppressLint("InlinedApi")
  @SuppressWarnings("deprecation")
  public static boolean areLocationServicesTurnedOn()
  {
    final ContentResolver resolver = MwmApplication.get().getContentResolver();
    try
    {
      return Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT
             ? !TextUtils.isEmpty(Settings.Secure.getString(resolver, Settings.Secure.LOCATION_PROVIDERS_ALLOWED))
             : Settings.Secure.getInt(resolver, Settings.Secure.LOCATION_MODE) != Settings.Secure.LOCATION_MODE_OFF;
    } catch (Settings.SettingNotFoundException e)
    {
      e.printStackTrace();
      return false;
    }
  }

  public static void logAvailableProviders()
  {
    LocationManager locMngr = (LocationManager) MwmApplication.get().getSystemService(Context.LOCATION_SERVICE);
    List<String> providers = locMngr.getProviders(true);
    StringBuilder sb;
    if (!providers.isEmpty())
    {
      sb = new StringBuilder("Available location providers:");
      for (String provider : providers)
        sb.append(" ").append(provider);
    }
    else
    {
      sb = new StringBuilder("There are no enabled location providers!");
    }
    LOGGER.i(TAG, sb.toString(), new Throwable());
  }
}
