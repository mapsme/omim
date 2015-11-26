package com.mapswithme.maps.location;

import android.content.ContentResolver;
import android.content.Context;
import android.hardware.*;
import android.location.Location;
import android.os.Build;
import android.preference.PreferenceManager;
import android.provider.Settings;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.text.TextUtils;
import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GoogleApiAvailability;
import com.mapswithme.maps.LocationState;
import com.mapswithme.maps.MwmApplication;
import com.mapswithme.maps.R;
import com.mapswithme.maps.bookmarks.data.MapObject;
import com.mapswithme.util.LocationUtils;
import com.mapswithme.util.concurrency.UiThread;
import com.mapswithme.util.log.Logger;
import com.mapswithme.util.log.SimpleLogger;

import java.util.HashSet;
import java.util.Set;

public enum LocationHelper implements SensorEventListener
{
  INSTANCE;

  protected final Logger mLogger;
  // These constants should correspond to values defined in platform/location.hpp
  // Leave 0-value as no any error.
  public static final int ERROR_NOT_SUPPORTED = 1;
  public static final int ERROR_DENIED = 2;
  public static final int ERROR_GPS_OFF = 3;

  public static final String LOCATION_PREDICTOR_PROVIDER = "LocationPredictorProvider";
  private static final long STOP_DELAY = 5000;

  public interface LocationListener
  {
    void onLocationUpdated(final Location l);

    void onCompassUpdated(long time, double magneticNorth, double trueNorth, double accuracy);

    void onLocationError(int errorCode);
  }

  private final Set<LocationListener> mListeners = new HashSet<>();

  private Location mLastLocation;
  private MapObject.MyPosition mMyPosition;
  private long mLastLocationTime;

  private final SensorManager mSensorManager;
  private Sensor mAccelerometer;
  private Sensor mMagnetometer;
  private GeomagneticField mMagneticField;
  private BaseLocationProvider mLocationProvider;

  private float[] mGravity;
  private float[] mGeomagnetic;
  private final float[] mR = new float[9];
  private final float[] mI = new float[9];
  private final float[] mOrientation = new float[3];

  private final Runnable mStopLocationTask = new Runnable() {
    @Override
    public void run()
    {
      mLocationProvider.stopUpdates();
      mMagneticField = null;
      if (mSensorManager != null)
        mSensorManager.unregisterListener(LocationHelper.this);
    }
  };

  LocationHelper()
  {
    mLogger = SimpleLogger.get(LocationHelper.class.getName());
    initLocationProvider(false);
    mSensorManager = (SensorManager) MwmApplication.get().getSystemService(Context.SENSOR_SERVICE);
    if (mSensorManager != null)
    {
      mAccelerometer = mSensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
      mMagnetometer = mSensorManager.getDefaultSensor(Sensor.TYPE_MAGNETIC_FIELD);
    }
  }

  @SuppressWarnings("deprecation")
  public void initLocationProvider(boolean forceNativeProvider)
  {
    boolean isLocationTurnedOn = false;

    final MwmApplication application = MwmApplication.get();
    // If location is turned off(by user in system settings), google client( = fused provider) api doesn't work at all
    // but external gps receivers still can work. In that case we prefer native provider instead of fused - it works.
    final ContentResolver resolver = application.getContentResolver();
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT)
    {
      final String providers = Settings.Secure.getString(resolver, Settings.Secure.LOCATION_PROVIDERS_ALLOWED);
      isLocationTurnedOn = !TextUtils.isEmpty(providers);
    }
    else
    {
      try
      {
        final int mode = Settings.Secure.getInt(resolver, Settings.Secure.LOCATION_MODE);
        isLocationTurnedOn = mode != Settings.Secure.LOCATION_MODE_OFF;
      } catch (Settings.SettingNotFoundException e)
      {
        e.printStackTrace();
      }
    }

    if (isLocationTurnedOn &&
        !forceNativeProvider &&
        GoogleApiAvailability.getInstance().isGooglePlayServicesAvailable(application) == ConnectionResult.SUCCESS &&
        PreferenceManager.getDefaultSharedPreferences(application).getBoolean(application.getString(R.string.pref_play_services), false))
    {
      mLogger.d("Use fused provider.");
      mLocationProvider = new GoogleFusedLocationProvider();
    }
    else
    {
      mLogger.d("Use native provider.");
      mLocationProvider = new AndroidNativeProvider();
    }

    if (!mListeners.isEmpty())
      mLocationProvider.startUpdates();
  }

  public @Nullable MapObject.MyPosition getMyPosition()
  {
    if (!LocationState.isTurnedOn())
    {
      invalidateLocation();
      return null;
    }

    if (mLastLocation == null)
      return null;

    if (mMyPosition == null)
      mMyPosition = new MapObject.MyPosition(mLastLocation.getLatitude(), mLastLocation.getLongitude());

    return mMyPosition;
  }

  public Location getLastLocation() { return mLastLocation; }

  public long getLastLocationTime() { return mLastLocationTime; }

  public void setLastLocation(@NonNull Location loc)
  {
    mLastLocation = loc;
    mMyPosition = null;
    mLastLocationTime = System.currentTimeMillis();
    notifyLocationUpdated();
  }

  public void invalidateLocation()
  {
    mLastLocation = null;
    mMyPosition = null;
  }

  void notifyLocationUpdated()
  {
    if (mLastLocation == null)
      return;

    for (LocationListener listener : mListeners)
      listener.onLocationUpdated(mLastLocation);
  }

  void notifyLocationError(int errCode)
  {
    for (LocationListener listener : mListeners)
      listener.onLocationError(errCode);
  }

  private void notifyCompassUpdated(long time, double magneticNorth, double trueNorth, double accuracy)
  {
    for (LocationListener listener : mListeners)
      listener.onCompassUpdated(time, magneticNorth, trueNorth, accuracy);
  }

  public void addLocationListener(LocationListener listener)
  {
    UiThread.cancelDelayedTasks(mStopLocationTask);
    if (mListeners.isEmpty())
      mLocationProvider.startUpdates();
    mListeners.add(listener);
    notifyLocationUpdated();
  }

  public void removeLocationListener(LocationListener listener)
  {
    mListeners.remove(listener);
    if (mListeners.isEmpty())
      // Make a delay with disconnection from location providers, so that orientation changes and short app sleeps
      // doesn't take long time to connect again.
      UiThread.runLater(mStopLocationTask, STOP_DELAY);
  }

  void registerSensorListeners()
  {
    if (mSensorManager != null)
    {
      final int COMPASS_REFRESH_MKS = SensorManager.SENSOR_DELAY_NORMAL;

      if (mAccelerometer != null)
        mSensorManager.registerListener(this, mAccelerometer, COMPASS_REFRESH_MKS);
      if (mMagnetometer != null)
        mSensorManager.registerListener(this, mMagnetometer, COMPASS_REFRESH_MKS);
    }
  }

  @Override
  public void onSensorChanged(SensorEvent event)
  {
    boolean hasOrientation = false;

    switch (event.sensor.getType())
    {
    case Sensor.TYPE_ACCELEROMETER:
      mGravity = nativeUpdateCompassSensor(0, event.values);
      break;
    case Sensor.TYPE_MAGNETIC_FIELD:
      mGeomagnetic = nativeUpdateCompassSensor(1, event.values);
      break;
    }

    if (mGravity != null && mGeomagnetic != null)
    {
      if (SensorManager.getRotationMatrix(mR, mI, mGravity, mGeomagnetic))
      {
        hasOrientation = true;
        SensorManager.getOrientation(mR, mOrientation);
      }
    }

    if (hasOrientation)
    {
      final double magneticHeading = LocationUtils.correctAngle(mOrientation[0], 0.0);

      if (mMagneticField == null)
        notifyCompassUpdated(event.timestamp, magneticHeading, -1.0, -1.0); // -1.0 - as default parameters
      else
      {
        // positive 'offset' means the magnetic field is rotated east that match from true north
        final double offset = Math.toRadians(mMagneticField.getDeclination());
        final double trueHeading = LocationUtils.correctAngle(magneticHeading, offset);

        notifyCompassUpdated(event.timestamp, magneticHeading, trueHeading, offset);
      }
    }
  }

  private native float[] nativeUpdateCompassSensor(int ind, float[] arr);

  @Override
  public void onAccuracyChanged(Sensor sensor, int accuracy) {}

  public void initMagneticField(Location newLocation)
  {
    if (mSensorManager != null)
    {
      // Recreate magneticField if location has changed significantly
      if (mMagneticField == null || mLastLocation == null ||
          newLocation.distanceTo(mLastLocation) > BaseLocationProvider.DISTANCE_TO_RECREATE_MAGNETIC_FIELD_M)
        mMagneticField = new GeomagneticField((float) newLocation.getLatitude(), (float) newLocation.getLongitude(),
            (float) newLocation.getAltitude(), newLocation.getTime());
    }
  }
}
