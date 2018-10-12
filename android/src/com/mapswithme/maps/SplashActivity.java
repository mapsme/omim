package com.mapswithme.maps;

import android.app.Activity;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.app.DialogFragment;
import android.support.v4.app.FragmentManager;
import android.support.v7.app.AlertDialog;
import android.support.v7.app.AppCompatActivity;
import android.view.View;

import com.mapswithme.maps.ads.Banner;
import com.mapswithme.maps.analytics.ExternalLibrariesMediator;
import com.mapswithme.maps.base.BaseActivity;
import com.mapswithme.maps.base.BaseActivityDelegate;
import com.mapswithme.maps.downloader.UpdaterDialogFragment;
import com.mapswithme.maps.editor.ViralFragment;
import com.mapswithme.maps.news.BaseNewsFragment;
import com.mapswithme.maps.news.NewsFragment;
import com.mapswithme.maps.news.WelcomeDialogFragment;
import com.mapswithme.maps.permissions.PermissionsDialogFragment;
import com.mapswithme.maps.permissions.StoragePermissionsDialogFragment;
import com.mapswithme.util.Config;
import com.mapswithme.util.Counters;
import com.mapswithme.util.PermissionsUtils;
import com.mapswithme.util.ThemeUtils;
import com.mapswithme.util.UiUtils;
import com.mapswithme.util.concurrency.UiThread;
import com.mapswithme.util.log.Logger;
import com.mapswithme.util.log.LoggerFactory;
import com.mapswithme.util.statistics.PushwooshHelper;
import com.my.tracker.MyTracker;

public class SplashActivity extends AppCompatActivity
    implements BaseNewsFragment.NewsDialogListener, BaseActivity
{
  private static final Logger LOGGER = LoggerFactory.INSTANCE.getLogger(LoggerFactory.Type.MISC);
  private static final String TAG = SplashActivity.class.getSimpleName();
  private static final String EXTRA_CORE_INITIALIZED = "extra_core_initialized";
  private static final String EXTRA_ACTIVITY_TO_START = "extra_activity_to_start";
  public static final String EXTRA_INITIAL_INTENT = "extra_initial_intent";
  private static final int REQUEST_PERMISSIONS = 1;
  private static final long FIRST_START_DELAY = 1000;
  private static final long DELAY = 100;

  // The first launch of application ever - onboarding screen will be shown.
  private static boolean sFirstStart;

  private View mIvLogo;
  private View mAppName;

  private boolean mPermissionsGranted;
  private boolean mNeedStoragePermission;
  private boolean mCanceled;

  @NonNull
  private final Runnable mPermissionsDelayedTask = new Runnable()
  {
    @Override
    public void run()
    {
      PermissionsDialogFragment.show(SplashActivity.this, REQUEST_PERMISSIONS);
    }
  };

  @NonNull
  private final Runnable mInitCoreDelayedTask = new Runnable()
  {
    @Override
    public void run()
    {
      MwmApplication app = (MwmApplication) getApplication();
      if (app.arePlatformAndCoreInitialized())
      {
        UiThread.runLater(mFinalDelayedTask);
        return;
      }

      ExternalLibrariesMediator mediator = ((MwmApplication) getApplicationContext()).getMediator();
      if (!mediator.isAdvertisingInfoObtained())
      {
        UiThread.runLater(mInitCoreDelayedTask, DELAY);
        return;
      }

      if (!mediator.isLimitAdTrackingEnabled())
      {
        LOGGER.i(TAG, "Limit ad tracking disabled, sensitive tracking initialized");
        mediator.initSensitiveData();
        MyTracker.trackLaunchManually(SplashActivity.this);
      }
      else
      {
        LOGGER.i(TAG, "Limit ad tracking enabled, sensitive tracking not initialized");
      }

      init();
      LOGGER.i(TAG, "Core initialized: " + app.arePlatformAndCoreInitialized());
      if (app.arePlatformAndCoreInitialized() && mediator.isLimitAdTrackingEnabled())
      {
        LOGGER.i(TAG, "Limit ad tracking enabled, rb banners disabled.");
        mediator.disableAdProvider(Banner.Type.TYPE_RB);
      }

//    Run delayed task because resumeDialogs() must see the actual value of mCanceled flag,
//    since onPause() callback can be blocked because of UI thread is busy with framework
//    initialization.
      UiThread.runLater(mFinalDelayedTask);
    }
  };

  @NonNull
  private final Runnable mFinalDelayedTask = new Runnable()
  {
    @Override
    public void run()
    {
      resumeDialogs();
    }
  };

  @NonNull
  private final BaseActivityDelegate mBaseDelegate = new BaseActivityDelegate(this);

  public static void start(@NonNull Context context,
                           @Nullable Class<? extends Activity> activityToStart,
                           @Nullable Intent initialIntent)
  {
    Intent intent = new Intent(context, SplashActivity.class);
    if (activityToStart != null)
      intent.putExtra(EXTRA_ACTIVITY_TO_START, activityToStart);
    if (initialIntent != null)
      intent.putExtra(EXTRA_INITIAL_INTENT, initialIntent);
    context.startActivity(intent);
  }

  public static boolean isFirstStart()
  {
    boolean res = sFirstStart;
    sFirstStart = false;
    return res;
  }

  @Override
  protected void onCreate(@Nullable Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);
    mBaseDelegate.onCreate();
    handleUpdateMapsFragmentCorrectly(savedInstanceState);
    UiThread.cancelDelayedTasks(mPermissionsDelayedTask);
    UiThread.cancelDelayedTasks(mInitCoreDelayedTask);
    UiThread.cancelDelayedTasks(mFinalDelayedTask);
    Counters.initCounters(this);
    initView();
  }

  @Override
  protected void onNewIntent(Intent intent)
  {
    super.onNewIntent(intent);
    mBaseDelegate.onNewIntent(intent);
  }

  private void handleUpdateMapsFragmentCorrectly(@Nullable Bundle savedInstanceState)
  {
    if (savedInstanceState == null)
      return;

    FragmentManager fm = getSupportFragmentManager();
    DialogFragment updaterFragment = (DialogFragment) fm
        .findFragmentByTag(UpdaterDialogFragment.class.getName());

    if (updaterFragment == null)
      return;

    // If the user revoked the external storage permission while the app was killed
    // we can't update maps automatically during recovering process, so just dismiss updater fragment
    // and ask the user to grant the permission.
    if (!PermissionsUtils.isExternalStorageGranted())
    {
      fm.beginTransaction().remove(updaterFragment).commitAllowingStateLoss();
      fm.executePendingTransactions();
      StoragePermissionsDialogFragment.show(this);
    }
    else
    {
      // If external permissions are still granted we just need to check platform
      // and core initialization, because we may be in the recovering process,
      // i.e. method onResume() may not be invoked in that case.
      if (!MwmApplication.get().arePlatformAndCoreInitialized())
      {
        init();
      }
    }
  }

  @Override
  protected void onStart()
  {
    super.onStart();
    mBaseDelegate.onStart();
  }

  @Override
  protected void onResume()
  {
    super.onResume();
    mBaseDelegate.onResume();
    mCanceled = false;
    mPermissionsGranted = PermissionsUtils.isExternalStorageGranted();
    DialogFragment storagePermissionsDialog = StoragePermissionsDialogFragment.find(this);
    DialogFragment permissionsDialog = PermissionsDialogFragment.find(this);
    if (!mPermissionsGranted)
    {
      if (mNeedStoragePermission || storagePermissionsDialog != null)
      {
        if (permissionsDialog != null)
          permissionsDialog.dismiss();
        if (storagePermissionsDialog == null)
          StoragePermissionsDialogFragment.show(this);
        return;
      }
      permissionsDialog = PermissionsDialogFragment.find(this);
      if (permissionsDialog == null)
        UiThread.runLater(mPermissionsDelayedTask, FIRST_START_DELAY);

      return;
    }

    if (permissionsDialog != null)
      permissionsDialog.dismiss();

    if (storagePermissionsDialog != null)
      storagePermissionsDialog.dismiss();

    UiThread.runLater(mInitCoreDelayedTask, DELAY);
  }

  @Override
  protected void onPause()
  {
    super.onPause();
    mBaseDelegate.onPause();
    mCanceled = true;
    UiThread.cancelDelayedTasks(mPermissionsDelayedTask);
    UiThread.cancelDelayedTasks(mInitCoreDelayedTask);
    UiThread.cancelDelayedTasks(mFinalDelayedTask);
  }

  @Override
  protected void onStop()
  {
    super.onStop();
    mBaseDelegate.onStop();
  }

  @Override
  protected void onDestroy()
  {
    super.onDestroy();
    mBaseDelegate.onDestroy();
  }

  private void resumeDialogs()
  {
    if (mCanceled)
      return;

    MwmApplication app = (MwmApplication) getApplication();
    if (!app.arePlatformAndCoreInitialized())
    {
      showExternalStorageErrorDialog();
      return;
    }

    if (Counters.isMigrationNeeded())
    {
      Config.migrateCountersToSharedPrefs();
      Counters.setMigrationExecuted();
    }

    sFirstStart = WelcomeDialogFragment.showOn(this);
    if (sFirstStart)
    {
      PushwooshHelper.nativeProcessFirstLaunch();
      UiUtils.hide(mIvLogo, mAppName);
      return;
    }

    boolean showNews = NewsFragment.showOn(this, this);
    if (!showNews)
    {
      if (ViralFragment.shouldDisplay())
      {
        UiUtils.hide(mIvLogo, mAppName);
        ViralFragment dialog = new ViralFragment();
        dialog.onDismissListener(new Runnable()
        {
          @Override
          public void run()
          {
            onDialogDone();
          }
        });
        dialog.show(getSupportFragmentManager(), "");
      }
      else
      {
        processNavigation();
      }
    }
    else
    {
      UiUtils.hide(mIvLogo, mAppName);
    }
  }

  private void showExternalStorageErrorDialog()
  {
    AlertDialog dialog = new AlertDialog.Builder(this)
        .setTitle(R.string.dialog_error_storage_title)
        .setMessage(R.string.dialog_error_storage_message)
        .setPositiveButton(R.string.ok, new DialogInterface.OnClickListener()
        {
          @Override
          public void onClick(DialogInterface dialog, int which)
          {
            SplashActivity.this.finish();
          }
        })
        .setCancelable(false)
        .create();
    dialog.show();
  }

  @Override
  public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions,
                                         @NonNull int[] grantResults)
  {
    super.onRequestPermissionsResult(requestCode, permissions, grantResults);
    if (grantResults.length == 0)
      return;

    mPermissionsGranted = PermissionsUtils.computePermissionsResult(permissions, grantResults)
                                          .isExternalStorageGranted();
    mNeedStoragePermission = !mPermissionsGranted;
  }

  @Override
  public void onDialogDone()
  {
    processNavigation();
  }

  private void initView()
  {
    UiUtils.setupStatusBar(this);
    setContentView(R.layout.activity_splash);
    mIvLogo = findViewById(R.id.iv__logo);
    mAppName = findViewById(R.id.tv__app_name);
  }

  private void init()
  {
    MwmApplication.get().initCore();
  }

  @SuppressWarnings("unchecked")
  private void processNavigation()
  {
    Intent input = getIntent();
    Intent result = new Intent(this, DownloadResourcesLegacyActivity.class);
    if (input != null)
    {
      if (input.hasExtra(EXTRA_ACTIVITY_TO_START))
      {
        result = new Intent(this,
                            (Class<? extends Activity>) input.getSerializableExtra(EXTRA_ACTIVITY_TO_START));
      }

      Intent initialIntent = input.hasExtra(EXTRA_INITIAL_INTENT) ?
                           input.getParcelableExtra(EXTRA_INITIAL_INTENT) :
                           input;
      result.putExtra(EXTRA_INITIAL_INTENT, initialIntent);
    }
    startActivity(result);
    finish();
  }

  @Override
  @NonNull
  public Activity get()
  {
    return this;
  }

  @Override
  public int getThemeResourceId(@NonNull String theme)
  {
    if (ThemeUtils.isDefaultTheme(theme))
      return R.style.MwmTheme;

    if (ThemeUtils.isNightTheme(theme))
      return R.style.MwmTheme_Night;

    throw new IllegalArgumentException("Attempt to apply unsupported theme: " + theme);
  }
}
