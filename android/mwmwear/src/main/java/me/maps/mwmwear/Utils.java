package me.maps.mwmwear;

public class Utils
{
  public static final int EARTH_RADIUS_METERS = 6378000;

  private Utils() {}

  public static double getDistanceMeters(double latSource, double lonSource, double latDest, double lonDest)
  {
    return EARTH_RADIUS_METERS * distanceOnSphere(Math.toRadians(latSource), Math.toRadians(lonSource),
        Math.toRadians(latDest), Math.toRadians(lonDest));
  }

  private static double distanceOnSphere(double latRadSource, double lonRadSource, double latRadDest, double lonRadDest)
  {
    final double dlat = Math.sin((latRadDest - latRadSource) / 2);
    final double dlon = Math.sin((lonRadDest - lonRadSource) / 2);
    final double y = dlat * dlat + dlon * dlon * Math.cos(latRadSource) * Math.cos(latRadDest);
    return 2 * Math.atan2(Math.sqrt(y), Math.sqrt(Math.max(.0, 1. - y)));
  }
}
