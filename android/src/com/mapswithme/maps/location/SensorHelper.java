package com.mapswithme.maps.location;

import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import com.mapswithme.maps.MwmApplication;
import com.mapswithme.util.log.Logger;
import com.mapswithme.util.log.LoggerFactory;

class SensorHelper implements SensorEventListener
{
  @Nullable
  private final SensorManager mSensorManager;
  @Nullable
  private Sensor mRotation;

  private static final int UNRELIABLE_MEASURES_COUNT_FOR_ALERT = 30;

  private int mUnreliableMeasuresCount = 0;

  private final static String TAG = LocationHelper.class.getSimpleName();
  private final Logger mLogger = LoggerFactory.INSTANCE.getLogger(LoggerFactory.Type.LOCATION);

  @Override
  public void onSensorChanged(SensorEvent event)
  {
    if (!MwmApplication.get().arePlatformAndCoreInitialized())
      return;

    notifyInternal(event);
  }

  private void manageCompassValues(@NonNull SensorEvent event)
  {
    float[] rotMatrix = new float[9];
    SensorManager.getRotationMatrixFromVector(rotMatrix, event.values);
    SensorManager.remapCoordinateSystem(rotMatrix,
            SensorManager.AXIS_X, SensorManager.AXIS_Y, rotMatrix);

    float[] rotVals = new float[3];
    SensorManager.getOrientation(rotMatrix, rotVals);

    // rotVals indexes: 0 - yaw, 2 - roll, 1 - pitch.
    LocationHelper.INSTANCE.notifyCompassUpdated(event.timestamp, rotVals[0]);
  }

  private void manageCompassAccuracy(@NonNull SensorEvent event)
  {
    mLogger.d(TAG, "SensorManager accuracy " + event.accuracy);
    if (event.accuracy == SensorManager.SENSOR_STATUS_ACCURACY_LOW ||
        event.accuracy == SensorManager.SENSOR_STATUS_UNRELIABLE)
    {
      mLogger.d(TAG, "SensorManager bad accuracy " + event.accuracy);
      mUnreliableMeasuresCount++;
      if (mUnreliableMeasuresCount == UNRELIABLE_MEASURES_COUNT_FOR_ALERT)
      {
        LocationHelper.INSTANCE.notifyCompassNeedsCalibration();
        mUnreliableMeasuresCount = 0;
      }
    }
    else
    {
      mUnreliableMeasuresCount = 0;
    }
  }

  private void notifyInternal(@NonNull SensorEvent event)
  {
    if (event.sensor.getType() == Sensor.TYPE_ROTATION_VECTOR)
    {
      manageCompassValues(event);
      manageCompassAccuracy(event);
    }
  }

  @Override
  public void onAccuracyChanged(Sensor sensor, int accuracy) {}

  SensorHelper()
  {
    mSensorManager = (SensorManager) MwmApplication.get().getSystemService(Context.SENSOR_SERVICE);

    if (mSensorManager != null)
      mRotation = mSensorManager.getDefaultSensor(Sensor.TYPE_ROTATION_VECTOR);
  }

  void start()
  {
    if (mRotation != null && mSensorManager != null)
      mSensorManager.registerListener(this, mRotation, SensorManager.SENSOR_DELAY_UI);
  }

  void stop()
  {
    if (mSensorManager != null)
      mSensorManager.unregisterListener(this);
  }
}
