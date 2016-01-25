package com.mapswithme.maps.editor;

import android.support.annotation.IntDef;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.Size;
import android.support.annotation.WorkerThread;

import com.mapswithme.maps.MwmApplication;

public final class OsmOAuth
{
  private OsmOAuth() {}

  // Result type corresponds to OsmOAuth::AuthResult.
  @IntDef({OK, FAIL_COOKIE, FAIL_LOGIN, NO_O_AUTH, FAIL_AUTH, NO_ACCESS, NETWORK_ERROR, SERVER_ERROR})
  public @interface AuthResult {}

  public static final int OK = 0;
  public static final int FAIL_COOKIE = 1;
  public static final int FAIL_LOGIN = 2;
  public static final int NO_O_AUTH = 3;
  public static final int FAIL_AUTH = 4;
  public static final int NO_ACCESS = 5;
  public static final int NETWORK_ERROR = 6;
  public static final int SERVER_ERROR = 7;

  private static final String PREF_OSM_TOKEN = "OsmToken";
  private static final String PREF_OSM_SECRET = "OsmSecret";

  public static boolean isAuthorized()
  {
    return MwmApplication.prefs().contains(PREF_OSM_TOKEN) && MwmApplication.prefs().contains(PREF_OSM_SECRET);
  }

  public static String getAuthToken()
  {
    return MwmApplication.prefs().getString(PREF_OSM_TOKEN, "");
  }

  public static String getAuthSecret()
  {
    return MwmApplication.prefs().getString(PREF_OSM_SECRET, "");
  }

  public static void setAuthorization(String token, String secret)
  {
    MwmApplication.prefs().edit()
                  .putString(PREF_OSM_TOKEN, token)
                  .putString(PREF_OSM_SECRET, secret)
                  .commit();
  }

  /**
   * @return array containing auth token and secret
   */
  @WorkerThread
  @Size(2)
  @Nullable
  public static native String[] nativeAuthWithPassword(String login, String password);

  /**
   * @return array containing auth token and secret
   */
  @WorkerThread
  @Size(2)
  @Nullable
  public static native String[] nativeAuthWithWebviewToken(String secret, String token, String verifier);

  /**
   * @return url for web auth, and token with secret for finishing authorization later
   */
  @Size(3)
  @NonNull
  public static native String[] nativeGetFacebookAuthUrl();

  /**
   * @return url for web auth, and token with secret for finishing authorization later
   */
  @Size(3)
  @NonNull
  public static native String[] nativeGetGoogleAuthUrl();
}
