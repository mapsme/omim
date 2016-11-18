package com.mapswithme.util;

import android.graphics.Color;
import android.net.TrafficStats;
import android.support.annotation.ColorInt;
import android.support.annotation.NonNull;
import android.util.Log;

import com.mapswithme.maps.BuildConfig;

public class NetworkUtils
{
  private static final String LOG_TAG = NetworkUtils.class.getSimpleName();

  private static Tag currentTag = Tag.DEFAULT;

  public enum Tag
  {
    DEFAULT(Color.RED),
    GPS_TRACKING(Color.CYAN),
    PARTNER_API(Color.GREEN),
    DOWNLOADER(Color.MAGENTA);

    @ColorInt
    private final int color;

    Tag(@ColorInt int color)
    {
      this.color = color;
    }
  }

  static void setNetworkTag()
  {
    setNetworkTag(Tag.DEFAULT);
  }

  public static void setNetworkTag(@NonNull Tag tag)
  {
    if (isNetworkDebugBuild())
    {
      Log.d(LOG_TAG, "setNetworkTag " + tag, new Throwable());
      TrafficStats.setThreadStatsTag(tag.color);
      currentTag = tag;
    }
  }

  private static boolean isNetworkDebugBuild()
  {
    return BuildConfig.BUILD_TYPE.equals("networkdebug");
  }

  public static void clearNetworkTag()
  {
    if (isNetworkDebugBuild())
    {
      Log.d(LOG_TAG, "clearNetworkTag " + currentTag, new Throwable());
      TrafficStats.clearThreadStatsTag();
    }
  }
}
