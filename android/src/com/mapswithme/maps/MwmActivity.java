package com.mapswithme.maps;

import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.location.Location;
import android.os.Bundle;
import android.os.Handler;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;
import android.support.v7.app.AlertDialog;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.Toast;
import com.mapswithme.country.ActiveCountryTree;
import com.mapswithme.country.DownloadActivity;
import com.mapswithme.country.DownloadFragment;
import com.mapswithme.country.StorageOptions;
import com.mapswithme.maps.Framework.OnBalloonListener;
import com.mapswithme.maps.MapStorage.Index;
import com.mapswithme.maps.activity.CustomNavigateUpListener;
import com.mapswithme.maps.ads.LikesManager;
import com.mapswithme.maps.api.ParsedMwmRequest;
import com.mapswithme.maps.base.BaseMwmFragmentActivity;
import com.mapswithme.maps.bookmarks.BookmarkCategoriesActivity;
import com.mapswithme.maps.bookmarks.ChooseBookmarkCategoryFragment;
import com.mapswithme.maps.bookmarks.data.BookmarkManager;
import com.mapswithme.maps.bookmarks.data.MapObject;
import com.mapswithme.maps.bookmarks.data.MapObject.ApiPoint;
import com.mapswithme.maps.dialog.RoutingErrorDialogFragment;
import com.mapswithme.maps.location.LocationHelper;
import com.mapswithme.maps.location.LocationPredictor;
import com.mapswithme.maps.routing.RoutingResultCodesProcessor;
import com.mapswithme.maps.search.SearchActivity;
import com.mapswithme.maps.search.SearchFragment;
import com.mapswithme.maps.search.FloatingSearchToolbarController;
import com.mapswithme.maps.settings.SettingsActivity;
import com.mapswithme.maps.settings.StoragePathManager;
import com.mapswithme.maps.settings.UnitLocale;
import com.mapswithme.maps.sound.TtsPlayer;
import com.mapswithme.maps.widget.FadeView;
import com.mapswithme.maps.widget.RoutingLayout;
import com.mapswithme.maps.widget.menu.MainMenu;
import com.mapswithme.maps.widget.placepage.BasePlacePageAnimationController;
import com.mapswithme.maps.widget.placepage.PlacePageView;
import com.mapswithme.maps.widget.placepage.PlacePageView.State;
import com.mapswithme.util.*;
import com.mapswithme.util.sharing.ShareOption;
import com.mapswithme.util.sharing.SharingHelper;
import com.mapswithme.util.statistics.AlohaHelper;
import com.mapswithme.util.statistics.Statistics;

import java.io.Serializable;
import java.util.Stack;


public class MwmActivity extends BaseMwmFragmentActivity
                      implements LocationHelper.LocationListener,
                                 OnBalloonListener,
                                 View.OnTouchListener,
                                 BasePlacePageAnimationController.OnVisibilityChangedListener,
                                 OnClickListener,
                                 Framework.RoutingListener,
                                 Framework.RoutingProgressListener,
                                 MapFragment.MapRenderingListener,
                                 CustomNavigateUpListener,
                                 ChooseBookmarkCategoryFragment.Listener
{
  public static final String EXTRA_TASK = "map_task";
  private static final String EXTRA_CONSUMED = "mwm.extra.intent.processed";
  private static final String EXTRA_UPDATE_COUNTRIES = ".extra.update.countries";

  private static final String FRAGMENT_TAG = MapFragment.class.getSimpleName();

  private static final String[] DOCKED_FRAGMENTS = {SearchFragment.class.getName(), DownloadFragment.class.getName()};

  // Instance state
  private static final String STATE_PP_OPENED = "PpOpened";
  private static final String STATE_MAP_OBJECT = "MapObject";

  // Map tasks that we run AFTER rendering initialized
  private final Stack<MapTask> mTasks = new Stack<>();
  private final StoragePathManager mPathManager = new StoragePathManager();

  private View mFrame;

  // map
  private MapFragment mMapFragment;
  // Place page
  private PlacePageView mPlacePage;
  // Routing
  private RoutingLayout mLayoutRouting;

  private MainMenu mMainMenu;
  private PanelAnimator mPanelAnimator;

  private FadeView mFadeView;

  private ImageButton mBtnZoomIn;
  private ImageButton mBtnZoomOut;

  private boolean mIsFragmentContainer;

  private LocationPredictor mLocationPredictor;
  private FloatingSearchToolbarController mSearchController;
  private LastCompassData mLastCompassData;

  public interface LeftAnimationTrackListener
  {
    void onTrackStarted(boolean collapsed);

    void onTrackFinished(boolean collapsed);

    void onTrackLeftAnimation(float offset);
  }

  private static class LastCompassData
  {
    double magneticNorth;
    double trueNorth;
    double north;

    void update(int rotation, double magneticNorth, double trueNorth)
    {
      this.magneticNorth = LocationUtils.correctCompassAngle(rotation, magneticNorth);
      this.trueNorth = LocationUtils.correctCompassAngle(rotation, trueNorth);
      north = (this.trueNorth >= 0.0) ? this.trueNorth : this.magneticNorth;
    }
  }

  public static Intent createShowMapIntent(Context context, Index index, boolean doAutoDownload)
  {
    return new Intent(context, DownloadResourcesActivity.class)
        .putExtra(DownloadResourcesActivity.EXTRA_COUNTRY_INDEX, index)
        .putExtra(DownloadResourcesActivity.EXTRA_AUTODOWNLOAD_COUNTRY, doAutoDownload);
  }

  public static void startSearch(Context context, String query)
  {
    final MwmActivity activity = (MwmActivity) context;
    if (activity.mIsFragmentContainer)
      activity.showSearch();
    else
      SearchActivity.startWithQuery(context, query);
  }

  public static Intent createUpdateMapsIntent()
  {
    return new Intent(MwmApplication.get(), MwmActivity.class)
        .putExtra(EXTRA_UPDATE_COUNTRIES, true);
  }

  @Override
  public void onRenderingInitialized()
  {
    checkMeasurementSystem();
    checkKitkatMigrationMove();

    runTasks();
  }

  private void runTasks()
  {
    // Task are not UI-thread bounded,
    // if any task need UI-thread it should implicitly
    // use Activity.runOnUiThread().
    while (!mTasks.isEmpty())
      mTasks.pop().run(this);
  }

  private static void checkMeasurementSystem()
  {
    UnitLocale.initializeCurrentUnits();
  }

  private void checkKitkatMigrationMove()
  {
    mPathManager.checkKitkatMigration(this);
  }

  @Override
  protected int getFragmentContentResId()
  {
    return (mIsFragmentContainer ? R.id.fragment_container
                                 : super.getFragmentContentResId());
  }

  void replaceFragmentInternal(Class<? extends Fragment> fragmentClass, Bundle args)
  {
    super.replaceFragment(fragmentClass, true, args);
  }

  @Override
  public void replaceFragment(Class<? extends Fragment> fragmentClass, boolean addToBackStack, Bundle args)
  {
    if (addToBackStack)
      mPanelAnimator.show(fragmentClass, args);
    else
      super.replaceFragment(fragmentClass, false, args);
  }

  private void showBookmarks()
  {
    startActivity(new Intent(this, BookmarkCategoriesActivity.class));
  }

  private void showSearch()
  {
    if (mIsFragmentContainer)
    {
      if (getSupportFragmentManager().findFragmentByTag(SearchFragment.class.getName()) == null)
        popFragment();

      mSearchController.hide();
      replaceFragment(SearchFragment.class, true, getIntent().getExtras());
    }
    else
      startActivity(new Intent(this, SearchActivity.class));
  }

  private void shareMyLocation()
  {
    final Location loc = LocationHelper.INSTANCE.getLastLocation();
    if (loc != null)
    {
      final String geoUrl = Framework.nativeGetGe0Url(loc.getLatitude(), loc.getLongitude(), Framework.getDrawScale(), "");
      final String httpUrl = Framework.getHttpGe0Url(loc.getLatitude(), loc.getLongitude(), Framework.getDrawScale(), "");
      final String body = getString(R.string.my_position_share_sms, geoUrl, httpUrl);
      ShareOption.ANY.share(this, body);
      return;
    }

    new AlertDialog.Builder(MwmActivity.this)
        .setMessage(R.string.unknown_current_position)
        .setCancelable(true)
        .setPositiveButton(android.R.string.ok, new Dialog.OnClickListener()
        {
          @Override
          public void onClick(DialogInterface dialog, int which)
          {
            dialog.dismiss();
          }
        }).show();
  }

  private void showDownloader(boolean openDownloadedList)
  {
    final Bundle args = new Bundle();
    args.putBoolean(DownloadActivity.EXTRA_OPEN_DOWNLOADED_LIST, openDownloadedList);
    if (mIsFragmentContainer)
    {
      if (getSupportFragmentManager().findFragmentByTag(DownloadFragment.class.getName()) != null) // downloader is already shown
        return;

      popFragment();
      FloatingSearchToolbarController.cancelSearch();
      mSearchController.refreshToolbar();
      replaceFragment(DownloadFragment.class, true, args);
    }
    else
    {
      final Intent intent = new Intent(this, DownloadActivity.class).putExtras(args);
      startActivity(intent);
    }
  }

  @Override
  public void onCreate(@Nullable Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);

    setContentView(R.layout.activity_map);
    initViews();

    // TODO consider implementing other model of listeners connection, without activities being bound
    Framework.nativeSetRoutingListener(this);
    Framework.nativeSetRouteProgressListener(this);
    Framework.nativeSetBalloonListener(this);

    mSearchController = new FloatingSearchToolbarController(this);
    mLocationPredictor = new LocationPredictor(new Handler(), this);
    processIntent(getIntent());
    SharingHelper.prepare();
  }

  private void initViews()
  {
    initMap();
    initYota();
    initPlacePage();
    initRoutingBox();
    initNavigationButtons();

    if (findViewById(R.id.fragment_container) != null)
      mIsFragmentContainer = true;

    initMenu();
  }

  private void initRoutingBox()
  {
    mLayoutRouting = (RoutingLayout) findViewById(R.id.layout__routing);
    mLayoutRouting.setListener(new RoutingLayout.ActionListener() {
      @Override
      public void onCloseRouting() {
        mMainMenu.setNavigationMode(false);
        adjustZoomButtons(false);
      }

      @Override
      public void onStartRouteFollow() {
        mMainMenu.setNavigationMode(true);
        adjustZoomButtons(true);
      }

      @Override
      public void onRouteTypeChange(int type) {
      }
    });
  }

  private void initMap()
  {
    mFrame = findViewById(R.id.map_fragment_container);

    mFadeView = (FadeView) findViewById(R.id.fade_view);
    mFadeView.setListener(new FadeView.Listener()
    {
      @Override
      public void onTouch()
      {
        mMainMenu.close(true);
      }
    });
    mMapFragment = (MapFragment) getSupportFragmentManager().findFragmentByTag(FRAGMENT_TAG);
    if (mMapFragment == null)
    {
      mMapFragment = (MapFragment) MapFragment.instantiate(this, MapFragment.class.getName(), null);
      getSupportFragmentManager()
          .beginTransaction()
          .replace(R.id.map_fragment_container, mMapFragment, FRAGMENT_TAG)
          .commit();
    }
    mFrame.setOnTouchListener(this);
  }

  private void initNavigationButtons()
  {
    View navigationButtons = findViewById(R.id.navigation_buttons);
    mBtnZoomIn = (ImageButton) navigationButtons.findViewById(R.id.map_button_plus);
    mBtnZoomIn.setOnClickListener(this);
    mBtnZoomOut = (ImageButton) navigationButtons.findViewById(R.id.map_button_minus);
    mBtnZoomOut.setOnClickListener(this);
  }

  private void initPlacePage()
  {
    mPlacePage = (PlacePageView) findViewById(R.id.info_box);
    mPlacePage.setOnVisibilityChangedListener(this);
    mPlacePage.findViewById(R.id.ll__route).setOnClickListener(this);
  }

  private void initYota()
  {
    if (Yota.isFirstYota())
      findViewById(R.id.yop_it).setOnClickListener(this);
  }

  private boolean closePlacePage()
  {
    if (mPlacePage.getState() == State.HIDDEN)
      return false;

    mPlacePage.hide();
    Framework.deactivatePopup();
    return true;
  }

  private boolean closeSidePanel()
  {
    if (canFragmentInterceptBackPress())
      return true;

    if (popFragment())
    {
      InputUtils.hideKeyboard(mFadeView);
      mFadeView.fadeOut(false);
      return true;
    }

    return false;
  }

  private void closeMenuAndRun(String statEvent, Runnable proc)
  {
    AlohaHelper.logClick(statEvent);

    mFadeView.fadeOut(false);
    mMainMenu.close(true, proc);
  }

  private void buildRoute()
  {
    closeMenuAndRun(AlohaHelper.PP_ROUTE, new Runnable() {
      @Override
      public void run() {
        mLayoutRouting.setEndPoint(mPlacePage.getMapObject());
        mLayoutRouting.setState(RoutingLayout.State.PREPARING, true);

        if (mPlacePage.isDocked() || !mPlacePage.isFloating())
          closePlacePage();
      }
    });
  }

  private void toggleMenu()
  {
    if (mMainMenu.isOpen())
      mFadeView.fadeOut(false);
    else
      mFadeView.fadeIn();

    mMainMenu.toggle(true);
  }

  private void initMenu()
  {
    mMainMenu = new MainMenu((ViewGroup) findViewById(R.id.menu_frame), new MainMenu.Container()
    {
      @Override
      public Activity getActivity()
      {
        return MwmActivity.this;
      }

      @Override
      public void onItemClick(MainMenu.Item item)
      {
        switch (item)
        {
        case TOGGLE:
          if (!mMainMenu.isOpen())
          {
            if (mPlacePage.isDocked() && closePlacePage())
              return;

            if (closeSidePanel())
              return;
          }

          AlohaHelper.logClick(AlohaHelper.TOOLBAR_MENU);
          toggleMenu();
          break;

        case SEARCH:
          closeMenuAndRun(AlohaHelper.TOOLBAR_SEARCH, new Runnable()
          {
            @Override
            public void run()
            {
              showSearch();
            }
          });
          break;

        /*case ROUTE:
          buildRoute();
          break;*/

        case BOOKMARKS:
          closeMenuAndRun(AlohaHelper.TOOLBAR_BOOKMARKS, new Runnable()
          {
            @Override
            public void run()
            {
              showBookmarks();
            }
          });
          break;

        case SHARE:
          closeMenuAndRun(AlohaHelper.MENU_SHARE, new Runnable()
          {
            @Override
            public void run()
            {
              shareMyLocation();
            }
          });
          break;

        case DOWNLOADER:
          closeMenuAndRun(AlohaHelper.MENU_DOWNLOADER, new Runnable()
          {
            @Override
            public void run()
            {
              showDownloader(false);
            }
          });
          break;

        case SETTINGS:
          closeMenuAndRun(AlohaHelper.MENU_SETTINGS, new Runnable()
          {
            @Override
            public void run()
            {
              startActivity(new Intent(getActivity(), SettingsActivity.class));
            }
          });
          break;
        }
      }
    });

    if (mPlacePage.isDocked())
    {
      mPlacePage.setLeftAnimationTrackListener(mMainMenu.getLeftAnimationTrackListener());
      return;
    }

    if (mIsFragmentContainer)
      mPanelAnimator = new PanelAnimator(this, mMainMenu.getLeftAnimationTrackListener());
  }

  @Override
  public void onDestroy()
  {
    Framework.nativeRemoveBalloonListener();
    BottomSheetHelper.free();
    super.onDestroy();
  }

  @Override
  protected void onSaveInstanceState(Bundle outState)
  {
    if (mPlacePage.getState() != State.HIDDEN)
    {
      outState.putBoolean(STATE_PP_OPENED, true);
      outState.putParcelable(STATE_MAP_OBJECT, mPlacePage.getMapObject());
    }

    mMainMenu.onSaveState(outState);
    super.onSaveInstanceState(outState);
  }

  @Override
  protected void onRestoreInstanceState(@NonNull Bundle savedInstanceState)
  {
    super.onRestoreInstanceState(savedInstanceState);

    if (savedInstanceState.getBoolean(STATE_PP_OPENED))
    {
      mPlacePage.setMapObject((MapObject) savedInstanceState.getParcelable(STATE_MAP_OBJECT));
      mPlacePage.setState(State.PREVIEW);
    }

    mMainMenu.onRestoreState(savedInstanceState);
    if (mMainMenu.shouldOpenDelayed())
      mFadeView.fadeInInstantly();

    mMainMenu.setNavigationMode(mLayoutRouting.getState() == RoutingLayout.State.TURN_INSTRUCTIONS);
  }

  @Override
  protected void onNewIntent(Intent intent)
  {
    super.onNewIntent(intent);
    processIntent(intent);
  }

  private void processIntent(Intent intent)
  {
    if (intent == null)
      return;

    if (intent.hasExtra(EXTRA_TASK))
      addTask(intent);
    else if (intent.hasExtra(EXTRA_UPDATE_COUNTRIES))
    {
      ActiveCountryTree.updateAll();
      showDownloader(true);
    }
  }

  private void addTask(Intent intent)
  {
    if (intent != null &&
        !intent.getBooleanExtra(EXTRA_CONSUMED, false) &&
        intent.hasExtra(EXTRA_TASK) &&
        ((intent.getFlags() & Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY) == 0))
    {
      final MapTask mapTask = (MapTask) intent.getSerializableExtra(EXTRA_TASK);
      mTasks.add(mapTask);
      intent.removeExtra(EXTRA_TASK);

      if (MapFragment.nativeIsEngineCreated())
        runTasks();

      // mark intent as consumed
      intent.putExtra(EXTRA_CONSUMED, true);
    }
  }

  @Override
  public void onLocationError(int errorCode)
  {
    MapFragment.nativeOnLocationError(errorCode);

    if (errorCode == LocationHelper.ERROR_DENIED)
    {
      LocationState.INSTANCE.turnOff();

      Intent intent = new Intent(android.provider.Settings.ACTION_LOCATION_SOURCE_SETTINGS);
      if (intent.resolveActivity(getPackageManager()) == null)
      {
        intent = new Intent(android.provider.Settings.ACTION_SECURITY_SETTINGS);
        if (intent.resolveActivity(getPackageManager()) == null)
          return;
      }

      final Intent finIntent = intent;
      new AlertDialog.Builder(this)
          .setTitle(R.string.enable_location_service)
          .setMessage(R.string.location_is_disabled_long_text)
          .setPositiveButton(R.string.connection_settings, new DialogInterface.OnClickListener()
          {
            @Override
            public void onClick(DialogInterface dialog, int which)
            {
              startActivity(finIntent);
            }
          })
          .setNegativeButton(R.string.close, null)
          .show();
    }
    else if (errorCode == LocationHelper.ERROR_GPS_OFF)
    {
      Toast.makeText(this, R.string.gps_is_disabled_long_text, Toast.LENGTH_LONG).show();
    }
  }

  @Override
  public void onLocationUpdated(final Location l)
  {
    if (!l.getProvider().equals(LocationHelper.LOCATION_PREDICTOR_PROVIDER))
      mLocationPredictor.reset(l);

    MapFragment.nativeLocationUpdated(l.getTime(), l.getLatitude(), l.getLongitude(), l.getAccuracy(), l.getAltitude(), l.getSpeed(), l.getBearing());

    if (mPlacePage.getState() != State.HIDDEN)
      mPlacePage.refreshLocation(l);

    RoutingLayout.State state = mLayoutRouting.getState();
    if (state != RoutingLayout.State.HIDDEN)
    {
      mLayoutRouting.updateRouteInfo();
      mMainMenu.updateRoutingInfo();

      if (state == RoutingLayout.State.TURN_INSTRUCTIONS)
        TtsPlayer.INSTANCE.playTurnNotifications();
    }
  }

  @Override
  public void onCompassUpdated(long time, double magneticNorth, double trueNorth, double accuracy)
  {
    if (mLastCompassData == null)
      mLastCompassData = new LastCompassData();

    mLastCompassData.update(getWindowManager().getDefaultDisplay().getRotation(), magneticNorth, trueNorth);

    MapFragment.nativeCompassUpdated(mLastCompassData.magneticNorth, mLastCompassData.trueNorth, false);
    if (mPlacePage.getState() != State.HIDDEN)
      mPlacePage.refreshAzimuth(mLastCompassData.north);

    if (mLayoutRouting.getState() != RoutingLayout.State.HIDDEN)
      mLayoutRouting.refreshAzimuth(mLastCompassData.north);
  }

  // Callback from native location state mode element processing.
  @SuppressWarnings("unused")
  public void onMyPositionModeChangedCallback(final int newMode)
  {
    mLocationPredictor.myPositionModeChanged(newMode);
    mMainMenu.getMyPositionButton().update(newMode);
    switch (newMode)
    {
    case LocationState.UNKNOWN_POSITION:
      pauseLocation();
      break;
    case LocationState.PENDING_POSITION:
      resumeLocation();
      break;
    default:
      break;
    }
  }

  private void listenLocationStateUpdates()
  {
    LocationState.INSTANCE.setMyPositionModeListener(this);
  }

  private static void stopLocationStateUpdates()
  {
    LocationState.INSTANCE.removeMyPositionModeListener();
  }

  @Override
  protected void onResume()
  {
    super.onResume();

    listenLocationStateUpdates();
    invalidateLocationState();
    adjustZoomButtons(Framework.nativeIsRoutingActive());

    mSearchController.refreshToolbar();
    mPlacePage.onResume();
    LikesManager.INSTANCE.showDialogs(this);
    mMainMenu.onResume();
  }

  @Override
  protected void onStart()
  {
    super.onStart();

    TtsPlayer.INSTANCE.reinitIfLocaleChanged();
    if (!mIsFragmentContainer)
      popFragment();
  }

  private void adjustZoomButtons(boolean routingActive)
  {
    boolean show = (routingActive || MwmApplication.get().nativeGetBoolean(SettingsActivity.ZOOM_BUTTON_ENABLED, true));
    UiUtils.showIf(show, mBtnZoomIn, mBtnZoomOut);

    if (!show)
      return;

    mFrame.post(new Runnable()
    {
      @Override
      public void run()
      {
        int height = mFrame.getMeasuredHeight();
        int top = UiUtils.dimen(R.dimen.zoom_buttons_top_required_space);
        int bottom = UiUtils.dimen(R.dimen.zoom_buttons_bottom_max_space);

        int space = (top + bottom < height ? bottom : height - top);

        LinearLayout.LayoutParams lp = (LinearLayout.LayoutParams) mBtnZoomOut.getLayoutParams();
        lp.bottomMargin = space;
        mBtnZoomOut.setLayoutParams(lp);
      }
    });
  }

  @Override
  protected void onPause()
  {
    stopLocationStateUpdates();
    pauseLocation();
    TtsPlayer.INSTANCE.stop();
    LikesManager.INSTANCE.cancelDialogs();
    super.onPause();
  }

  private void resumeLocation()
  {
    LocationHelper.INSTANCE.addLocationListener(this);
    // Do not turn off the screen while displaying position
    Utils.keepScreenOn(true, getWindow());
    mLocationPredictor.resume();
  }

  private void pauseLocation()
  {
    LocationHelper.INSTANCE.removeLocationListener(this);
    // Enable automatic turning screen off while app is idle
    Utils.keepScreenOn(false, getWindow());
    mLocationPredictor.pause();
  }

  private void refreshLocationState(int newMode)
  {
    mMainMenu.getMyPositionButton().update(newMode);

    switch (newMode)
    {
      case LocationState.UNKNOWN_POSITION:
        pauseLocation();
        break;
      case LocationState.PENDING_POSITION:
        resumeLocation();
        break;
      default:
        break;
    }
  }

  /**
   * Invalidates location state in core.
   * Updates location button accordingly.
   */
  public void invalidateLocationState()
  {
    final int currentLocationMode = LocationState.INSTANCE.getLocationStateMode();
    refreshLocationState(currentLocationMode);
    LocationState.INSTANCE.invalidatePosition();
  }

  @Override
  public void onBackPressed()
  {
    if (mMainMenu.close(true))
    {
      mFadeView.fadeOut(false);
      return;
    }

    if (mSearchController.hide())
    {
      FloatingSearchToolbarController.cancelSearch();
      return;
    }

    if (!closePlacePage() && !closeSidePanel() && !closeRouting())
      super.onBackPressed();
  }

  private boolean isMapFaded()
  {
    return mFadeView.getVisibility() == View.VISIBLE;
  }

  private boolean canFragmentInterceptBackPress()
  {
    final FragmentManager manager = getSupportFragmentManager();
    DownloadFragment fragment = (DownloadFragment) manager.findFragmentByTag(DownloadFragment.class.getName());
    return fragment != null && fragment.isResumed() && fragment.onBackPressed();
  }

  private boolean popFragment()
  {
    for (String tag : DOCKED_FRAGMENTS)
      if (popFragment(tag))
        return true;

    return false;
  }

  private boolean popFragment(String className)
  {
    final FragmentManager manager = getSupportFragmentManager();
    Fragment fragment = manager.findFragmentByTag(className);

    // TODO d.yunitsky
    // we cant pop fragment, if it isn't resumed, cause of 'at android.support.v4.app.FragmentManagerImpl.checkStateLoss(FragmentManager.java:1375)'
    if (fragment == null || !fragment.isResumed())
      return false;

    if (mPanelAnimator == null)
      return false;

    mPanelAnimator.hide(new Runnable()
    {
      @Override
      public void run()
      {
        manager.popBackStackImmediate();
      }
    });

    return true;
  }

  // Callbacks from native map objects touch event.
  @Override
  public void onApiPointActivated(final double lat, final double lon, final String name, final String id)
  {
    if (ParsedMwmRequest.hasRequest())
    {
      final ParsedMwmRequest request = ParsedMwmRequest.getCurrentRequest();
      request.setPointData(lat, lon, name, id);

      runOnUiThread(new Runnable()
      {
        @Override
        public void run()
        {
          final String poiType = ParsedMwmRequest.getCurrentRequest().getCallerName(MwmApplication.get()).toString();
          activateMapObject(new ApiPoint(name, id, poiType, lat, lon));
        }
      });
    }
  }

  @Override
  public void onPoiActivated(final String name, final String type, final String address, final double lat, final double lon,
                             final int[] metaTypes, final String[] metaValues)
  {
    final MapObject poi = new MapObject.Poi(name, lat, lon, type);
    poi.addMetadata(metaTypes, metaValues);

    runOnUiThread(new Runnable()
    {
      @Override
      public void run()
      {
        activateMapObject(poi);
      }
    });
  }

  @Override
  public void onBookmarkActivated(final int category, final int bookmarkIndex)
  {
    runOnUiThread(new Runnable()
    {
      @Override
      public void run()
      {
        activateMapObject(BookmarkManager.INSTANCE.getBookmark(category, bookmarkIndex));
      }
    });
  }

  @Override
  public void onMyPositionActivated(final double lat, final double lon)
  {
    final MapObject mypos = new MapObject.MyPosition(getString(R.string.my_position), lat, lon);

    runOnUiThread(new Runnable()
    {
      @Override
      public void run()
      {
        if (!Framework.nativeIsRoutingActive())
        {
          activateMapObject(mypos);
        }
      }
    });
  }

  @Override
  public void onAdditionalLayerActivated(final String name, final String type, final double lat, final double lon, final int[] metaTypes, final String[] metaValues)
  {
    runOnUiThread(new Runnable()
    {
      @Override
      public void run()
      {
        final MapObject sr = new MapObject.SearchResult(name, type, lat, lon);
        sr.addMetadata(metaTypes, metaValues);
        activateMapObject(sr);
      }
    });
  }

  private void activateMapObject(MapObject object)
  {
    if (!mPlacePage.hasMapObject(object))
    {
      mPlacePage.setMapObject(object);
      mPlacePage.setState(State.PREVIEW);

      if (isMapFaded())
        mFadeView.fadeOut(false);
    }
  }

  @Override
  public void onDismiss()
  {
    if (!mPlacePage.hasMapObject(null))
    {
      runOnUiThread(new Runnable()
      {
        @Override
        public void run()
        {
          mPlacePage.hide();
        }
      });
    }
  }

  @Override
  public void onPreviewVisibilityChanged(boolean isVisible)
  {
    if (!isVisible)
    {
      Framework.deactivatePopup();
      mPlacePage.setMapObject(null);
      mMainMenu.show(true);
    }
  }

  @Override
  public void onPlacePageVisibilityChanged(boolean isVisible)
  {
    AlohaHelper.logClick(isVisible ? AlohaHelper.PP_OPEN
                                   : AlohaHelper.PP_CLOSE);
  }

  @Override
  public void onClick(View v)
  {
    switch (v.getId())
    {
    case R.id.ll__route:
      buildRoute();
      break;
    case R.id.map_button_plus:
      AlohaHelper.logClick(AlohaHelper.ZOOM_IN);
      MapFragment.nativeScalePlus();
      break;
    case R.id.map_button_minus:
      AlohaHelper.logClick(AlohaHelper.ZOOM_OUT);
      MapFragment.nativeScaleMinus();
      break;
    case R.id.yop_it:
      final double[] latLon = Framework.getScreenRectCenter();
      final double zoom = Framework.getDrawScale();

      final int locationStateMode = LocationState.INSTANCE.getLocationStateMode();

      if (locationStateMode > LocationState.NOT_FOLLOW)
        Yota.showLocation(getApplicationContext(), zoom);
      else
        Yota.showMap(getApplicationContext(), latLon[0], latLon[1], zoom, null, locationStateMode == LocationState.NOT_FOLLOW);

      Statistics.INSTANCE.trackBackscreenCall("Map");
      break;
    }
  }

  private boolean closeRouting()
  {
    if (mLayoutRouting.getState() == RoutingLayout.State.HIDDEN)
      return false;

    mLayoutRouting.setState(RoutingLayout.State.HIDDEN, true);
    mMainMenu.setNavigationMode(false);
    adjustZoomButtons(false);
    return true;
  }

  @Override
  public boolean onTouch(View view, MotionEvent event)
  {
    return mPlacePage.hideOnTouch() ||
        mMapFragment.onTouch(view, event);
  }

  @Override
  public void onRoutingEvent(final int resultCode, final Index[] missingCountries, final Index[] missingRoutes)
  {
    runOnUiThread(new Runnable()
    {
      @Override
      public void run()
      {
        if (resultCode == RoutingResultCodesProcessor.NO_ERROR)
          mLayoutRouting.setState(RoutingLayout.State.ROUTE_BUILT, true);
        else
        {
          mLayoutRouting.setState(RoutingLayout.State.ROUTE_BUILD_ERROR, true);
          final Bundle args = new Bundle();
          args.putInt(RoutingErrorDialogFragment.EXTRA_RESULT_CODE, resultCode);
          args.putSerializable(RoutingErrorDialogFragment.EXTRA_MISSING_COUNTRIES, missingCountries);
          args.putSerializable(RoutingErrorDialogFragment.EXTRA_MISSING_ROUTES, missingRoutes);
          final RoutingErrorDialogFragment fragment = (RoutingErrorDialogFragment) Fragment.instantiate(MwmActivity.this, RoutingErrorDialogFragment.class.getName());
          fragment.setArguments(args);
          fragment.setListener(new RoutingErrorDialogFragment.RoutingDialogListener()
          {
            @Override
            public void onDownload()
            {
              mLayoutRouting.setState(RoutingLayout.State.HIDDEN, false);
              if (missingCountries != null && missingCountries.length != 0)
                ActiveCountryTree.downloadMapsForIndex(missingCountries, StorageOptions.MAP_OPTION_MAP_AND_CAR_ROUTING);
              if (missingRoutes != null && missingRoutes.length != 0)
                ActiveCountryTree.downloadMapsForIndex(missingRoutes, StorageOptions.MAP_OPTION_CAR_ROUTING);
              showDownloader(true);
            }

            @Override
            public void onCancel()
            {}

            @Override
            public void onOk()
            {
              if (RoutingResultCodesProcessor.isDownloadable(resultCode))
              {
                mLayoutRouting.setState(RoutingLayout.State.HIDDEN, false);
                showDownloader(false);
              }
            }
          });
          fragment.show(getSupportFragmentManager(), RoutingErrorDialogFragment.class.getName());
        }
      }
    });
  }

  @Override
  public void onRouteBuildingProgress(final float progress)
  {
    runOnUiThread(new Runnable()
    {
      @Override
      public void run()
      {
        mLayoutRouting.setRouteBuildingProgress(progress);
      }
    });
  }

  @Override
  public void customOnNavigateUp()
  {
    if (popFragment())
    {
      InputUtils.hideKeyboard(mMainMenu.getFrame());
      mSearchController.refreshToolbar();
    }
  }

  public interface MapTask extends Serializable
  {
    boolean run(MwmActivity target);
  }

  public static class OpenUrlTask implements MapTask
  {
    private static final long serialVersionUID = 1L;
    private final String mUrl;

    public OpenUrlTask(String url)
    {
      Utils.checkNotNull(url);
      mUrl = url;
    }

    @Override
    public boolean run(MwmActivity target)
    {
      return MapFragment.nativeShowMapForUrl(mUrl);
    }
  }

  public static class ShowCountryTask implements MapTask
  {
    private static final long serialVersionUID = 1L;
    private final Index mIndex;
    private final boolean mDoAutoDownload;

    public ShowCountryTask(Index index, boolean doAutoDownload)
    {
      mIndex = index;
      mDoAutoDownload = doAutoDownload;
    }

    @Override
    public boolean run(MwmActivity target)
    {
      if (mDoAutoDownload)
      {
        Framework.downloadCountry(mIndex);
        // set zoom level so that download process is visible
        Framework.nativeShowCountry(mIndex, true);
      }
      else
        Framework.nativeShowCountry(mIndex, false);

      return true;
    }
  }

  public void adjustCompass(int offset)
  {
    if (mMapFragment == null || !mMapFragment.isAdded())
      return;

    mMapFragment.adjustCompass(mPanelAnimator.isVisible() ? offset : 0, true);

    if (mLastCompassData != null)
      MapFragment.nativeCompassUpdated(mLastCompassData.magneticNorth, mLastCompassData.trueNorth, true);
  }

  @Override
  public void onCategoryChanged(int bookmarkId, int newCategoryId)
  {
    mPlacePage.setMapObject(BookmarkManager.INSTANCE.getBookmark(newCategoryId, bookmarkId));
  }
}
