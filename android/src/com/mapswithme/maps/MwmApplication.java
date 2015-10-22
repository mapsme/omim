package com.mapswithme.maps;

import android.content.SharedPreferences;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Environment;
import android.os.Handler;
import android.preference.PreferenceManager;
import android.util.Log;

import com.google.gson.Gson;
import com.mapswithme.country.ActiveCountryTree;
import com.mapswithme.country.CountryItem;
import com.mapswithme.maps.background.Notifier;
import com.mapswithme.maps.bookmarks.data.BookmarkManager;
import com.mapswithme.util.Config;
import com.mapswithme.util.Constants;
import com.mapswithme.util.UiUtils;
import com.mapswithme.util.Yota;
import com.mapswithme.util.statistics.AlohaHelper;
import com.mapswithme.util.statistics.Statistics;
import com.parse.Parse;
import com.parse.ParseException;
import com.parse.ParseInstallation;
import com.parse.SaveCallback;

import java.io.File;

public class MwmApplication extends android.app.Application implements ActiveCountryTree.ActiveCountryListener
{
  private final static String TAG = "MwmApplication";

  // Parse
  private static final String PREF_PARSE_DEVICE_TOKEN = "ParseDeviceToken";
  private static final String PREF_PARSE_INSTALLATION_ID = "ParseInstallationId";

  private static MwmApplication mSelf;
  private final Gson mGson = new Gson();
  private static SharedPreferences mPrefs;

  private boolean mAreStatsInitialised;
  private Handler mMainLoopHandler;

  public MwmApplication()
  {
    super();
    mSelf = this;
  }

  public static MwmApplication get()
  {
    return mSelf;
  }

  public static Gson gson()
  {
    return mSelf.mGson;
  }

  public static SharedPreferences prefs()
  {
    return mPrefs;
  }

  @Override
  public void onCountryProgressChanged(int group, int position, long[] sizes) {}

  @Override
  public void onCountryStatusChanged(int group, int position, int oldStatus, int newStatus)
  {
    Notifier.cancelDownloadSuggest();
    if (newStatus == MapStorage.DOWNLOAD_FAILED)
    {
      CountryItem item = ActiveCountryTree.getCountryItem(group, position);
      Notifier.notifyDownloadFailed(ActiveCountryTree.getCoreIndex(group, position), item.getName());
    }
  }

  @Override
  public void onCountryGroupChanged(int oldGroup, int oldPosition, int newGroup, int newPosition)
  {
    if (oldGroup == ActiveCountryTree.GROUP_NEW && newGroup == ActiveCountryTree.GROUP_UP_TO_DATE)
      Statistics.INSTANCE.myTrackerTrackMapDownload();
    else if (oldGroup == ActiveCountryTree.GROUP_OUT_OF_DATE && newGroup == ActiveCountryTree.GROUP_UP_TO_DATE)
      Statistics.INSTANCE.myTrackerTrackMapUpdate();
  }

  @Override
  public void onCountryOptionsChanged(int group, int position, int newOptions, int requestOptions) {}

  @Override
  public void onCreate()
  {
    super.onCreate();

    mMainLoopHandler = new Handler(getMainLooper());

    final String extStoragePath = getDataStoragePath();
    final String extTmpPath = getTempPath();

    new File(extStoragePath).mkdirs();
    new File(extTmpPath).mkdirs();
  }

  private void initNativeStrings()
  {
    nativeAddLocalization("country_status_added_to_queue", getString(R.string.country_status_added_to_queue));
    nativeAddLocalization("country_status_downloading", getString(R.string.country_status_downloading));
    nativeAddLocalization("country_status_download", getString(R.string.country_status_download));
    nativeAddLocalization("country_status_download_without_routing", getString(R.string.country_status_download_without_routing));
    nativeAddLocalization("country_status_download_failed", getString(R.string.country_status_download_failed));
    nativeAddLocalization("try_again", getString(R.string.try_again));
    nativeAddLocalization("not_enough_free_space_on_sdcard", getString(R.string.not_enough_free_space_on_sdcard));
    nativeAddLocalization("dropped_pin", getString(R.string.dropped_pin));
    nativeAddLocalization("my_places", getString(R.string.my_places));
    nativeAddLocalization("my_position", getString(R.string.my_position));
    nativeAddLocalization("routes", getString(R.string.routes));

    nativeAddLocalization("routing_failed_unknown_my_position", getString(R.string.routing_failed_unknown_my_position));
    nativeAddLocalization("routing_failed_has_no_routing_file", getString(R.string.routing_failed_has_no_routing_file));
    nativeAddLocalization("routing_failed_start_point_not_found", getString(R.string.routing_failed_start_point_not_found));
    nativeAddLocalization("routing_failed_dst_point_not_found", getString(R.string.routing_failed_dst_point_not_found));
    nativeAddLocalization("routing_failed_cross_mwm_building", getString(R.string.routing_failed_cross_mwm_building));
    nativeAddLocalization("routing_failed_route_not_found", getString(R.string.routing_failed_route_not_found));
    nativeAddLocalization("routing_failed_internal_error", getString(R.string.routing_failed_internal_error));
  }

  public String getApkPath()
  {
    try
    {
      return getPackageManager().getApplicationInfo(BuildConfig.APPLICATION_ID, 0).sourceDir;
    } catch (final NameNotFoundException e)
    {
      Log.e(TAG, "Can't get apk path from PackageManager");
      return "";
    }
  }

  public String getDataStoragePath()
  {
    return Environment.getExternalStorageDirectory().getAbsolutePath() + Constants.MWM_DIR_POSTFIX;
  }

  public String getTempPath()
  {
    final File cacheDir = getExternalCacheDir();
    if (cacheDir != null)
      return cacheDir.getAbsolutePath();

    return Environment.getExternalStorageDirectory().getAbsolutePath() +
        String.format(Constants.STORAGE_PATH, BuildConfig.APPLICATION_ID, Constants.CACHE_DIR);
  }

  private String getObbGooglePath()
  {
    final String storagePath = Environment.getExternalStorageDirectory().getAbsolutePath();
    return storagePath.concat(String.format(Constants.OBB_PATH, BuildConfig.APPLICATION_ID));
  }

  static
  {
    System.loadLibrary("mapswithme");
  }

  public void runNativeFunctorOnUIThread(final long functionPointer)
  {
    mMainLoopHandler.post(new Runnable()
    {
      @Override
      public void run()
      {
        nativeCallOnUIThread(functionPointer);
      }
    });
  }

  private native void nativeInit(String apkPath, String storagePath,
                                 String tmpPath, String obbGooglePath,
                                 String flavorName, String buildType,
                                 boolean isYota, boolean isTablet);

  private native void nativeCallOnUIThread(long functorPointer);
  private native void nativeAddLocalization(String name, String value);

  /**
   * Check if device have at least {@code size} bytes free.
   */
  public native boolean hasFreeSpace(long size);

  /*
   * init Parse SDK
   */
  private void initParse()
  {
    Parse.initialize(this, PrivateVariables.parseApplicationId(), PrivateVariables.parseClientKey());
    ParseInstallation.getCurrentInstallation().saveInBackground(new SaveCallback()
    {
      @Override
      public void done(ParseException e)
      {
        SharedPreferences prefs = prefs();
        String previousId = prefs.getString(PREF_PARSE_INSTALLATION_ID, "");
        String previousToken = prefs.getString(PREF_PARSE_DEVICE_TOKEN, "");

        String newId = ParseInstallation.getCurrentInstallation().getInstallationId();
        String newToken = ParseInstallation.getCurrentInstallation().getString("deviceToken");
        if (!previousId.equals(newId) || !previousToken.equals(newToken))
        {
          org.alohalytics.Statistics.logEvent(AlohaHelper.PARSE_INSTALLATION_ID, newId);
          org.alohalytics.Statistics.logEvent(AlohaHelper.PARSE_DEVICE_TOKEN, newToken);
          prefs.edit()
               .putString(PREF_PARSE_INSTALLATION_ID, newId)
               .putString(PREF_PARSE_DEVICE_TOKEN, newToken).apply();
        }
      }
    });
  }

  public void initCounters()
  {
    if (!mAreCountersInitialised)
    {
      mAreCountersInitialised = true;
      Config.updateLaunchCounter();
      PreferenceManager.setDefaultValues(this, R.xml.prefs_misc, false);
    }
  }

  public void onUpgrade()
  {
    Config.resetAppSessionCounters();
  }
}
