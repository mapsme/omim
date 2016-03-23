package com.mapswithme.util;

import com.mapswithme.maps.BuildConfig;

public final class Constants
{
  public static final String STORAGE_PATH = "/Android/data/%s/%s/";
  public static final String OBB_PATH = "/Android/obb/%s/";

  public static final int KB = 1024;
  public static final int MB = 1024 * 1024;

  public static class Url
  {
    public static final String GE0_PREFIX = "ge0://";
    public static final String MAILTO_SCHEME = "mailto:";
    public static final String MAIL_SUBJECT = "?subject=";
    public static final String MAIL_BODY = "&body=";
    public static final String HTTP_GE0_PREFIX = "http://ge0.me/";

    public static final String PLAY_MARKET_HTTPS_APP_PREFIX = "https://play.google.com/store/apps/details?id=";

    public static final String FB_MAPSME_COMMUNITY_HTTP = "http://www.facebook.com/MapsWithMe";
    // Profile id is taken from http://graph.facebook.com/MapsWithMe
    public static final String FB_MAPSME_COMMUNITY_NATIVE = "fb://profile/111923085594432";
    public static final String TWITTER_MAPSME_HTTP = "https://twitter.com/MAPS_ME";

    public static final String WEB_SITE = "http://maps.me";
    public static final String WEB_BLOG = "http://blog.maps.me";

    public static final String COPYRIGHT = "file:///android_asset/copyright.html";
    public static final String FAQ = "file:///android_asset/faq.html";

    private Url() {}
  }

  public static class Email
  {
    public static final String FEEDBACK = "android@maps.me";
    public static final String SUPPORT = BuildConfig.SUPPORT_MAIL;
    public static final String SUBSCRIBE = "subscribe@maps.me";
    public static final String RATING = "rating@maps.me";

    private Email() {}
  }

  public static class Package
  {
    public static final String FB_PACKAGE = "com.facebook.katana";
    public static final String MWM_PRO_PACKAGE = "com.mapswithme.maps.pro";
    public static final String MWM_LITE_PACKAGE = "com.mapswithme.maps";
    public static final String MWM_SAMSUNG_PACKAGE = "com.mapswithme.maps.samsung";
    public static final String TWITTER_PACKAGE = "com.twitter.android";

    private Package() {}
  }


  public static final String MWM_DIR_POSTFIX = "/MapsWithMe/";
  public static final String DEVICE_YOTAPHONE = "yotaphone";
  public static final String CACHE_DIR = "cache";
  public static final String FILES_DIR = "files";

  private Constants() {}
}
