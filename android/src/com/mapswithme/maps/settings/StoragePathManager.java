package com.mapswithme.maps.settings;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Environment;
import android.support.annotation.Nullable;
import android.text.TextUtils;
import android.util.Log;

import com.mapswithme.maps.BuildConfig;
import com.mapswithme.maps.Framework;
import com.mapswithme.maps.MwmApplication;
import com.mapswithme.maps.R;
import com.mapswithme.util.Config;
import com.mapswithme.util.Constants;
import com.mapswithme.util.UiUtils;
import com.mapswithme.util.concurrency.ThreadPool;
import com.mapswithme.util.concurrency.UiThread;

import java.io.File;
import java.io.FilenameFilter;
import java.util.ArrayList;
import java.util.List;

public class StoragePathManager
{
  private static final String[] MOVABLE_EXTS = Framework.nativeGetMovableFilesExts();
  private static final FilenameFilter MOVABLE_FILES_FILTER = new FilenameFilter()
  {
    @Override
    public boolean accept(File dir, String filename)
    {
      for (String ext : MOVABLE_EXTS)
        if (filename.endsWith(ext))
          return true;

      return false;
    }
  };

  public interface MoveFilesListener
  {
    void moveFilesFinished(String newPath);

    void moveFilesFailed(String errorCode);
  }

  public interface OnStorageListChangedListener
  {
    void onStorageListChanged(List<String> storageItems);
  }

  static final String TAG = StoragePathManager.class.getName();

  private OnStorageListChangedListener mStoragesChangedListener;
  private MoveFilesListener mMoveFilesListener;

  private BroadcastReceiver mStorageStateReceiver;
  private Activity mActivity;
  private List<String> mItems = new ArrayList<>();

  /**
   * Observes status of connected media and retrieves list of available external storages.
   */
  public void startExternalStorageWatching(Activity activity, @Nullable OnStorageListChangedListener storagesChangedListener,
      @Nullable MoveFilesListener moveFilesListener)
  {
    mActivity = activity;
    mStoragesChangedListener = storagesChangedListener;
    mMoveFilesListener = moveFilesListener;
    mStorageStateReceiver = new BroadcastReceiver()
    {
      @Override
      public void onReceive(Context context, Intent intent)
      {
        updateStorages();

        if (mStoragesChangedListener != null)
          mStoragesChangedListener.onStorageListChanged(mItems);
      }
    };

    mActivity.registerReceiver(mStorageStateReceiver, getMediaChangesIntentFilter());
    updateStorages();
  }

  public static IntentFilter getMediaChangesIntentFilter()
  {
    final IntentFilter filter = new IntentFilter();
    filter.addAction(Intent.ACTION_MEDIA_MOUNTED);
    filter.addAction(Intent.ACTION_MEDIA_REMOVED);
    filter.addAction(Intent.ACTION_MEDIA_EJECT);
    filter.addAction(Intent.ACTION_MEDIA_SHARED);
    filter.addAction(Intent.ACTION_MEDIA_UNMOUNTED);
    filter.addAction(Intent.ACTION_MEDIA_BAD_REMOVAL);
    filter.addAction(Intent.ACTION_MEDIA_UNMOUNTABLE);
    filter.addAction(Intent.ACTION_MEDIA_CHECKING);
    filter.addAction(Intent.ACTION_MEDIA_NOFS);
    filter.addDataScheme(ContentResolver.SCHEME_FILE);

    return filter;
  }

  public void stopExternalStorageWatching()
  {
    if (mStorageStateReceiver != null)
    {
      mActivity.unregisterReceiver(mStorageStateReceiver);
      mStorageStateReceiver = null;
      mStoragesChangedListener = null;
    }
  }

  public List<String> getStorages()
  {
    return mItems;
  }

  public void updateStorages()
  {
    mItems.clear();
    // current storage
    mItems.add(getMwmPathRoot());
    // primary external storage
    mItems.add(Environment.getExternalStorageDirectory().getAbsolutePath());
    // all other parsed paths from multiple configs
    mItems.addAll(StorageUtils.parseStorages());
    mItems = filterStorages(mItems);
  }

  public void changeStorage(final String newPath)
  {
    final String oldPath = getMwmPath();

    final File f = new File(oldPath);
    if (!f.exists() && !f.mkdirs())
    {
      Log.e(TAG, "Can't move files. Old directory isn't accessible : " + oldPath);
      return;
    }

    setStoragePath(newPath, new MoveFilesListener()
    {
      @Override
      public void moveFilesFinished(String newPath)
      {
        updateStorages();
        if (mMoveFilesListener != null)
          mMoveFilesListener.moveFilesFinished(newPath);
      }

      @Override
      public void moveFilesFailed(String errorCode)
      {
        updateStorages();
        if (mMoveFilesListener != null)
          mMoveFilesListener.moveFilesFailed(errorCode);
      }
    });
  }

  public static void resetStorage(final String newPath)
  {
    Framework.nativeSetWritableDir(newPath);
  }

  /**
   * Checks whether current directory is actually writable on Kitkat devices. On earlier versions of android ( < 4.4 ) the root of external
   * storages was writable, but on Kitkat it isn't, so we should move our maps to other directory.
   * http://www.doubleencore.com/2014/03/android-external-storage/ check that link for explanations
   * <p/>
   * TODO : use SAF framework to allow custom sdcard folder selections on Lollipop+ devices.
   * https://developer.android.com/guide/topics/providers/document-provider.html#client
   * https://code.google.com/p/android/issues/detail?id=103249
   */
  public void checkKitkatMigration(final Activity activity)
  {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT ||
        Config.isKitKatMigrationComplete() ||
        isMwmPathWritable())
      return;

    final long size = getMwmDirSize();
    updateStorages();
    for (String path : mItems)
    {
      if (StorageUtils.getFreeBytesAtPath(path) < size)
        continue;

      setStoragePath(path, new MoveFilesListener()
      {
        @Override
        public void moveFilesFinished(String newPath)
        {
          Config.setKitKatMigrationComplete();
          UiUtils.showAlertDialog(activity, R.string.kitkat_migrate_ok);
        }

        @Override
        public void moveFilesFailed(String errorCode)
        {
          UiUtils.showAlertDialog(activity, R.string.kitkat_migrate_failed);
        }
      });

      return;
    }

    UiUtils.showAlertDialog(activity, R.string.kitkat_migrate_failed);
  }

  private void setStoragePath(final String newPath, final MoveFilesListener listener)
  {
    final File oldStorage = new File(getMwmPath());
    ThreadPool.getStorage().execute(new Runnable()
    {
      @Override
      public void run()
      {
        final String result = StorageUtils.moveFiles(new File(newPath), oldStorage, MOVABLE_FILES_FILTER);

        UiThread.run(new Runnable()
        {
          @Override
          public void run()
          {
            if (TextUtils.isEmpty(result))
            {
              Framework.nativeSetWritableDir(newPath);
              listener.moveFilesFinished(newPath);
            }
            else
              listener.moveFilesFailed(result);

            updateStorages();
          }
        });
      }
    });
  }

  @SuppressWarnings("ResultOfMethodCallIgnored")
  public static void initPaths()
  {
    new File(getSettingsPath()).mkdirs();
    new File(getTempPath()).mkdirs();
  }

  /**
   * @return path to settings.ini. Always stored on {@link Constants#MWM_DIR_POSTFIX} path on primary external storage.
   * Bookmark .kml files are stored here, too.
   */
  public static String getSettingsPath()
  {
    return Environment.getExternalStorageDirectory().getAbsolutePath() + Constants.MWM_DIR_POSTFIX;
  }

  /**
   * @return path to mwm-s(maps) and other large files. May be primary external storage or sdcard.
   */
  public static String getMwmPath()
  {
    return Framework.nativeGetWritableDir();
  }

  public static String getMwmPathRoot()
  {
    String writableDir = getMwmPath();
    int index = writableDir.lastIndexOf(Constants.MWM_DIR_POSTFIX);
    if (index != -1)
      writableDir = writableDir.substring(0, index);

    return writableDir;
  }

  public static long getMwmDirSize()
  {
    return StorageUtils.getDirSizeRecursively(new File(Framework.nativeGetWritableDir()), MOVABLE_FILES_FILTER);
  }

  public static String getApkPath()
  {
    try
    {
      return MwmApplication.get().getPackageManager().getApplicationInfo(BuildConfig.APPLICATION_ID, 0).sourceDir;
    } catch (final PackageManager.NameNotFoundException e)
    {
      Log.e(TAG, "Can't get apk path from PackageManager");
      return "";
    }
  }

  public static String getTempPath()
  {
    final File cacheDir = MwmApplication.get().getExternalCacheDir();
    if (cacheDir != null)
      return cacheDir.getAbsolutePath();

    return Environment.getExternalStorageDirectory().getAbsolutePath() +
        String.format(Constants.STORAGE_PATH, BuildConfig.APPLICATION_ID, Constants.CACHE_DIR);
  }

  public static String getObbGooglePath()
  {
    final String storagePath = Environment.getExternalStorageDirectory().getAbsolutePath();
    return storagePath.concat(String.format(Constants.OBB_PATH, BuildConfig.APPLICATION_ID));
  }

  public static boolean isMwmPathWritable()
  {
    final String settingsPath = getSettingsPath();
    final String mwmPath = getMwmPath();

    return settingsPath.equals(mwmPath) || StorageUtils.isPathWritable(mwmPath);
  }

  /**
   * Filters duplicating storages by its names and free sizes.
   */
  private static List<String> filterStorages(List<String> storagesWithDuplicates)
  {
    final List<String> storages = new ArrayList<>();
    final List<Long> sizes = new ArrayList<>();

    for (String storage : storagesWithDuplicates)
    {
      final long freeSize = StorageUtils.getFreeBytesAtPath(storage);

      if (storages.contains(storage) || sizes.contains(freeSize))
        continue;

      storages.add(storage);
      sizes.add(freeSize);
    }

    return storages;
  }
}
