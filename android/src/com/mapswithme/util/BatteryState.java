package com.mapswithme.util;

import android.content.Intent;
import android.content.IntentFilter;
import android.os.BatteryManager;
import android.support.annotation.IntDef;
import android.support.annotation.IntRange;
import android.support.annotation.NonNull;

import com.mapswithme.maps.MwmApplication;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

public final class BatteryState
{
  public static final byte CHARGING_STATUS_UNKNOWN = 0;
  public static final byte CHARGING_STATUS_PLUGGED = 1;
  public static final byte CHARGING_STATUS_UNPLUGGED = 2;

  @Retention(RetentionPolicy.SOURCE)
  @IntDef({ CHARGING_STATUS_UNKNOWN, CHARGING_STATUS_PLUGGED, CHARGING_STATUS_UNPLUGGED })
  public @interface ChargingStatus {}

  private BatteryState() {}

  @NonNull
  public static State getState()
  {
    IntentFilter filter = new IntentFilter(Intent.ACTION_BATTERY_CHANGED);
    // Because it's a sticky intent, you don't need to register a BroadcastReceiver
    // by simply calling registerReceiver passing in null
    Intent batteryStatus = MwmApplication.get().registerReceiver(null, filter);
    if (batteryStatus == null)
      return new State(0, CHARGING_STATUS_UNKNOWN);

    return new State(getLevel(batteryStatus), getChargingStatus(batteryStatus));
  }

  @ChargingStatus
  public static int getChargingStatus()
  {
    return getState().mChargingStatus;
  }

  @IntRange(from=0, to=100)
  private static int getLevel(@NonNull Intent batteryStatus)
  {
    return batteryStatus.getIntExtra(BatteryManager.EXTRA_LEVEL, 0);
  }

  @ChargingStatus
  private static int getChargingStatus(@NonNull Intent batteryStatus)
  {
    // Extra for {@link android.content.Intent#ACTION_BATTERY_CHANGED}:
    // integer indicating whether the device is plugged in to a power
    // source; 0 means it is on battery, other constants are different
    // types of power sources.
    int chargePlug = batteryStatus.getIntExtra(BatteryManager.EXTRA_PLUGGED, -1);
    if (chargePlug > 0)
      return CHARGING_STATUS_PLUGGED;
    else if (chargePlug < 0)
      return CHARGING_STATUS_UNKNOWN;

    return CHARGING_STATUS_UNPLUGGED;
  }

  public static final class State
  {
    @IntRange(from=0, to=100)
    private final int mLevel;

    @ChargingStatus
    private final int mChargingStatus;

    public State(@IntRange(from=0, to=100) int level, @ChargingStatus int chargingStatus)
    {
      mLevel = level;
      mChargingStatus = chargingStatus;
    }

    @IntRange(from=0, to=100)
    public int getLevel()
    {
      return mLevel;
    }

    @ChargingStatus
    public int getChargingStatus()
    {
      return mChargingStatus;
    }
  }
}
