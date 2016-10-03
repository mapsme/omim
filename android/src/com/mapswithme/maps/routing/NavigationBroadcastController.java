package com.mapswithme.maps.routing;

import android.content.Context;
import android.content.Intent;
import android.location.Location;
import android.text.TextUtils;
import android.util.Pair;

import com.mapswithme.maps.Framework;
import com.mapswithme.maps.bookmarks.data.DistanceAndAzimut;
import com.mapswithme.maps.location.LocationHelper;
import com.mapswithme.util.StringUtils;

public class NavigationBroadcastController {

  private static final String START_ACTION = "com.mapswithme.maps.routing.START";
  private static final String STOP_ACTION = "com.mapswithme.maps.routing.STOP";
  private static final String UPDATE_ACTION = "com.mapswithme.maps.routing.UPDATE";

  // Common extras.
  private static final String EXTRA_ROUTER_TYPE = "router_type";
  private static final String EXTRA_NEXT_STREET = "next_street";
  private static final String EXTRA_DISTANCE_TO_TARGET = "distance_to_target";
  private static final String EXTRA_TARGET_UNITS = "target_units";
  private static final String EXTRA_COMPLETION_PERCENT = "completion_percent";
  private static final String EXTRA_TIME_LEFT = "time_left";
  private static final String EXTRA_SPEED = "speed";
  private static final String EXTRA_SPEED_UNITS = "speed_units";
  private static final String EXTRA_NORTH = "north";

  // Pedestrian extras.
  private static final String EXTRA_DISTANCE = "distance";
  private static final String EXTRA_AZIMUTH = "azimuth";

  // Vehicle extras.
  private static final String EXTRA_DISTANCE_TO_TURN = "distance_to_turn";
  private static final String EXTRA_DISTANCE_TO_TURN_UNITS = "distance_to_turn_units";
  private static final String EXTRA_TURN_DIRECTION = "turn_direction";
  private static final String EXTRA_NEXT_TURN_DIRECTION = "next_turn_direction";

  private static double north;

  public static void setNorth(final double north) {
    NavigationBroadcastController.north = north;
  }

  public static void sendIsNavigating(final Context context, final boolean isNavigating) {
    final Intent intent = new Intent(isNavigating ? START_ACTION : STOP_ACTION);
    intent.setFlags(Intent.FLAG_INCLUDE_STOPPED_PACKAGES);
    context.sendBroadcast(intent);
  }

  public static void sendUpdate(final Context context, final RoutingInfo info) {
    if (info == null) {
      return;
    }

    final Intent intent = new Intent(UPDATE_ACTION);
    intent.setFlags(Intent.FLAG_INCLUDE_STOPPED_PACKAGES);
    intent.putExtra(EXTRA_ROUTER_TYPE, Framework.nativeGetRouter());
    intent.putExtra(EXTRA_DISTANCE_TO_TARGET, info.distToTarget);
    intent.putExtra(EXTRA_TARGET_UNITS, info.targetUnits);
    intent.putExtra(EXTRA_COMPLETION_PERCENT, info.completionPercent);
    intent.putExtra(EXTRA_TIME_LEFT, info.totalTimeInSeconds);
    intent.putExtra(EXTRA_NORTH, north);

    if (!TextUtils.isEmpty(info.nextStreet)) {
      intent.putExtra(EXTRA_NEXT_STREET, info.nextStreet);
    }

    final Location last = LocationHelper.INSTANCE.getLastKnownLocation();
    if (last != null) {
      Pair<String, String> speedAndUnits = StringUtils.nativeFormatSpeedAndUnits(last.getSpeed());
      intent.putExtra(EXTRA_SPEED, speedAndUnits.first);
      intent.putExtra(EXTRA_SPEED_UNITS, speedAndUnits.second);
    }

    if (Framework.nativeGetRouter() == Framework.ROUTER_TYPE_PEDESTRIAN) {
      updatePedestrian(intent, info);
    } else {
      updateVehicle(intent, info);
    }

    context.sendBroadcast(intent);
  }

  private static void updatePedestrian(final Intent intent, final RoutingInfo info) {
    final Location next = info.pedestrianNextDirection;
    final Location location = LocationHelper.INSTANCE.getSavedLocation();
    // Zero is intentionally passed to the north parameter to obtain true azimuth.
    // North is passed as the separate extra.
    final DistanceAndAzimut distanceAndAzimuth = Framework.nativeGetDistanceAndAzimuthFromLatLon(
            next.getLatitude(), next.getLongitude(),
            location.getLatitude(), location.getLongitude(),
            0.0);
    intent.putExtra(EXTRA_DISTANCE, distanceAndAzimuth.getDistance());
    intent.putExtra(EXTRA_AZIMUTH, distanceAndAzimuth.getAzimuth());
  }

  private static void updateVehicle(final Intent intent, final RoutingInfo info) {
    intent.putExtra(EXTRA_DISTANCE_TO_TURN, info.distToTurn);
    intent.putExtra(EXTRA_DISTANCE_TO_TURN_UNITS, info.turnUnits);
    intent.putExtra(EXTRA_TURN_DIRECTION, info.vehicleTurnDirection.name());
    if (info.vehicleNextTurnDirection.containsNextTurn()) {
      intent.putExtra(EXTRA_NEXT_TURN_DIRECTION, info.vehicleNextTurnDirection.name());
    }
  }

  private NavigationBroadcastController() {
    // Do nothing.
  }
}
