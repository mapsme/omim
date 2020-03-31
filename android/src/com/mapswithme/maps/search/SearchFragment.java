package com.mapswithme.maps.search;

import android.app.Activity;
import android.content.Intent;
import android.location.Location;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.CallSuper;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.viewpager.widget.ViewPager;
import com.google.android.material.appbar.AppBarLayout;
import com.google.android.material.appbar.CollapsingToolbarLayout;
import com.google.android.material.tabs.TabLayout;
import com.mapswithme.maps.Framework;
import com.mapswithme.maps.MwmActivity;
import com.mapswithme.maps.R;
import com.mapswithme.maps.base.BaseMwmFragment;
import com.mapswithme.maps.base.OnBackPressListener;
import com.mapswithme.maps.bookmarks.data.FeatureId;
import com.mapswithme.maps.bookmarks.data.MapObject;
import com.mapswithme.maps.downloader.CountrySuggestFragment;
import com.mapswithme.maps.downloader.MapManager;
import com.mapswithme.maps.location.LocationHelper;
import com.mapswithme.maps.location.LocationListener;
import com.mapswithme.maps.routing.RoutingController;
import com.mapswithme.maps.widget.PlaceholderView;
import com.mapswithme.maps.widget.SearchToolbarController;
import com.mapswithme.util.SharedPropertiesUtils;
import com.mapswithme.util.UiUtils;
import com.mapswithme.util.Utils;
import com.mapswithme.util.log.Logger;
import com.mapswithme.util.log.LoggerFactory;
import com.mapswithme.util.statistics.Statistics;
import com.pushwoosh.Pushwoosh;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class SearchFragment extends BaseMwmFragment
                         implements OnBackPressListener,
                                    NativeSearchListener,
                                    SearchToolbarController.Container,
                                    CategoriesAdapter.CategoriesUiListener,
                                    HotelsFilterHolder, NativeBookingFilterListener
{
  private long mLastQueryTimestamp;
  @NonNull
  private final List<HiddenCommand> mHiddenCommands = new ArrayList<>();

  private static class LastPosition
  {
    double lat;
    double lon;
    boolean valid;

    public void set(double lat, double lon)
    {
      this.lat = lat;
      this.lon = lon;
      valid = true;
    }
  }

  private class ToolbarController extends SearchToolbarController
  {
    public ToolbarController(View root)
    {
      super(root, SearchFragment.this.getActivity());
    }

    @Override
    protected boolean useExtendedToolbar()
    {
      return false;
    }

    @Override
    protected void onTextChanged(String query)
    {
      if (!isAdded())
        return;

      if (TextUtils.isEmpty(query))
      {
        mSearchAdapter.clear();
        stopSearch();
        return;
      }

      if (tryRecognizeHiddenCommand(query))
      {
        mSearchAdapter.clear();
        stopSearch();
        closeSearch();
        return;
      }

      runSearch();
    }

    @Override
    protected boolean onStartSearchClick()
    {
      if (!RoutingController.get().isWaitingPoiPick())
        showAllResultsOnMap();
      return true;
    }

    @Override
    protected int getVoiceInputPrompt()
    {
      return R.string.search_map;
    }

    @Override
    protected void startVoiceRecognition(Intent intent, int code)
    {
      startActivityForResult(intent, code);
    }

    @Override
    protected boolean supportsVoiceSearch()
    {
      return true;
    }

    @Override
    public void onUpClick()
    {
      if (!onBackPressed())
        super.onUpClick();
    }

    @Override
    public void clear()
    {
      super.clear();
      if (mFilterController != null)
        mFilterController.resetFilter();
    }
  }

  private View mTabFrame;
  private View mResultsFrame;
  private PlaceholderView mResultsPlaceholder;
  private RecyclerView mResults;
  private AppBarLayout mAppBarLayout;
  private CollapsingToolbarLayout mToolbarLayout;
  @Nullable
  private SearchFilterController mFilterController;

  private SearchToolbarController mToolbarController;

  @SuppressWarnings("NullableProblems")
  @NonNull
  private SearchAdapter mSearchAdapter;

  private final List<RecyclerView> mAttachedRecyclers = new ArrayList<>();
  private final RecyclerView.OnScrollListener mRecyclerListener = new RecyclerView.OnScrollListener()
  {
    @Override
    public void onScrollStateChanged(RecyclerView recyclerView, int newState)
    {
      if (newState == RecyclerView.SCROLL_STATE_DRAGGING)
        mToolbarController.deactivate();
    }
  };

  private final LastPosition mLastPosition = new LastPosition();
  private boolean mSearchRunning;
  private String mInitialQuery;
  @Nullable
  private String mInitialLocale;
  private boolean mInitialSearchOnMap = false;
  @Nullable
  private HotelsFilter mInitialHotelsFilter;
  @Nullable
  private BookingFilterParams mInitialFilterParams;

  private final LocationListener mLocationListener = new LocationListener.Simple()
  {
    @Override
    public void onLocationUpdated(Location location)
    {
      mLastPosition.set(location.getLatitude(), location.getLongitude());

      if (!TextUtils.isEmpty(getQuery()))
        mSearchAdapter.notifyDataSetChanged();
    }
  };

  private final AppBarLayout.OnOffsetChangedListener mOffsetListener =
      new AppBarLayout.OnOffsetChangedListener() {
        @Override
        public void onOffsetChanged(AppBarLayout appBarLayout, int verticalOffset)
        {
          if (mFilterController == null)
            return;

          boolean show = !(Math.abs(verticalOffset) == appBarLayout.getTotalScrollRange());
          mFilterController.showDivider(show);
        }
      };

  private static boolean doShowDownloadSuggest()
  {
    return (MapManager.nativeGetDownloadedCount() == 0 && !MapManager.nativeIsDownloading());
  }

  @Override
  @Nullable
  public HotelsFilter getHotelsFilter()
  {
    if (mFilterController == null)
      return null;

    return mFilterController.getFilter();
  }

  @Nullable
  @Override
  public BookingFilterParams getFilterParams()
  {
    if (mFilterController == null)
      return null;

    return mFilterController.getBookingFilterParams();
  }

  private void showDownloadSuggest()
  {
    final FragmentManager fm = getChildFragmentManager();
    final String fragmentName = CountrySuggestFragment.class.getName();
    Fragment fragment = fm.findFragmentByTag(fragmentName);

    if (fragment == null || fragment.isDetached() || fragment.isRemoving())
    {
      fragment = Fragment.instantiate(getActivity(), fragmentName, null);
      fm.beginTransaction()
        .add(R.id.download_suggest_frame, fragment, fragmentName)
        .commit();
    }
  }

  private void hideDownloadSuggest()
  {
    if (!isAdded())
      return;

    final FragmentManager manager = getChildFragmentManager();
    final Fragment fragment = manager.findFragmentByTag(CountrySuggestFragment.class.getName());
    if (fragment != null && !fragment.isDetached() && !fragment.isRemoving())
      manager.beginTransaction()
             .remove(fragment)
             .commitAllowingStateLoss();
  }

  private void updateFrames()
  {
    final boolean hasQuery = mToolbarController.hasQuery();
    UiUtils.showIf(hasQuery, mResultsFrame);
    AppBarLayout.LayoutParams lp = (AppBarLayout.LayoutParams) mToolbarLayout.getLayoutParams();
    lp.setScrollFlags(hasQuery ? AppBarLayout.LayoutParams.SCROLL_FLAG_ENTER_ALWAYS
                        | AppBarLayout.LayoutParams.SCROLL_FLAG_SCROLL : 0);
    mToolbarLayout.setLayoutParams(lp);
    if (mFilterController != null)
      mFilterController.show(hasQuery, !RoutingController.get().isWaitingPoiPick());

    if (hasQuery)
      hideDownloadSuggest();
    else if (doShowDownloadSuggest())
      showDownloadSuggest();
    else
      hideDownloadSuggest();
  }

  private void updateResultsPlaceholder()
  {
    final boolean show = !mSearchRunning
                         && mSearchAdapter.getItemCount() == 0
                         && mToolbarController.hasQuery();

    UiUtils.showIf(show, mResultsPlaceholder);
    if (mFilterController != null)
      mFilterController.showPopulateButton(!RoutingController.get()
                                                             .isWaitingPoiPick() && !isTabletSearch());
  }

  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
  {
    return inflater.inflate(R.layout.fragment_search, container, false);
  }

  @CallSuper
  @Override
  public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState)
  {
    super.onViewCreated(view, savedInstanceState);
    mSearchAdapter = new SearchAdapter(this);
    readArguments();

    ViewGroup root = (ViewGroup) view;
    mAppBarLayout = root.findViewById(R.id.app_bar);
    mToolbarLayout = mAppBarLayout.findViewById(R.id.collapsing_toolbar);
    mTabFrame = root.findViewById(R.id.tab_frame);
    ViewPager pager = mTabFrame.findViewById(R.id.pages);

    mToolbarController = new ToolbarController(view);

    TabLayout tabLayout = root.findViewById(R.id.tabs);
    final TabAdapter tabAdapter = new TabAdapter(getChildFragmentManager(), pager, tabLayout);

    mResultsFrame = root.findViewById(R.id.results_frame);
    mResults = mResultsFrame.findViewById(R.id.recycler);
    setRecyclerScrollListener(mResults);
    mResultsPlaceholder = mResultsFrame.findViewById(R.id.placeholder);
    mResultsPlaceholder.setContent(R.drawable.img_search_nothing_found_light,
                                   R.string.search_not_found, R.string.search_not_found_query);
    mFilterController = new SearchFilterController(root.findViewById(R.id.filter_frame),
                                                   new SearchFilterController.DefaultFilterListener()
    {
      @Override
      public void onShowOnMapClick()
      {
        showAllResultsOnMap();
      }

      @Override
      public void onFilterClick()
      {
        HotelsFilter filter = null;
        BookingFilterParams params = null;
        if (mFilterController != null)
        {
          filter = mFilterController.getFilter();
          params = mFilterController.getBookingFilterParams();
        }
        FilterActivity.startForResult(SearchFragment.this, filter, params,
                                      FilterActivity.REQ_CODE_FILTER);
      }

      @Override
      public void onFilterClear()
      {
        runSearch();
      }
    });
    if (savedInstanceState != null)
      mFilterController.onRestoreState(savedInstanceState);
    if (mInitialHotelsFilter != null || mInitialFilterParams != null)
      mFilterController.setFilterAndParams(mInitialHotelsFilter, mInitialFilterParams);
    mFilterController.updateFilterButtonVisibility(mInitialFilterParams != null);

    mSearchAdapter.registerAdapterDataObserver(new RecyclerView.AdapterDataObserver()
    {
      @Override
      public void onChanged()
      {
        updateResultsPlaceholder();
      }
    });

    mResults.setLayoutManager(new LinearLayoutManager(view.getContext()));
    mResults.setAdapter(mSearchAdapter);

    updateFrames();
    updateResultsPlaceholder();

    if (mInitialQuery != null)
    {
      setQuery(mInitialQuery);
    }
    mToolbarController.activate();

    SearchEngine.INSTANCE.addListener(this);

    if (SearchRecents.getSize() == 0)
      pager.setCurrentItem(TabAdapter.Tab.CATEGORIES.ordinal());

    tabAdapter.setTabSelectedListener(tab ->
                                      {
                                        Statistics.INSTANCE.trackSearchTabSelected(tab.name());
                                        mToolbarController.deactivate();
                                      });

    if (mInitialSearchOnMap)
      showAllResultsOnMap();
  }

  @Override
  public void onSaveInstanceState(Bundle outState)
  {
    if (mFilterController != null)
      mFilterController.onSaveState(outState);
  }

  public void onResume()
  {
    super.onResume();
    LocationHelper.INSTANCE.addListener(mLocationListener, true);
    SearchEngine.INSTANCE.addHotelListener(this);
    mAppBarLayout.addOnOffsetChangedListener(mOffsetListener);
  }

  @Override
  public void onPause()
  {
    LocationHelper.INSTANCE.removeListener(mLocationListener);
    SearchEngine.INSTANCE.removeHotelListener(this);
    super.onPause();
    mAppBarLayout.removeOnOffsetChangedListener(mOffsetListener);
  }

  @Override
  public void onDestroy()
  {
    for (RecyclerView v : mAttachedRecyclers)
      v.removeOnScrollListener(mRecyclerListener);

    mAttachedRecyclers.clear();
    SearchEngine.INSTANCE.removeListener(this);
    super.onDestroy();
  }

  private String getQuery()
  {
    return mToolbarController.getQuery();
  }

  void setQuery(String text)
  {
    mToolbarController.setQuery(text);
  }

  private void readArguments()
  {
    final Bundle arguments = getArguments();
    if (arguments == null)
      return;

    mInitialQuery = arguments.getString(SearchActivity.EXTRA_QUERY);
    mInitialLocale = arguments.getString(SearchActivity.EXTRA_LOCALE);
    mInitialSearchOnMap = arguments.getBoolean(SearchActivity.EXTRA_SEARCH_ON_MAP);
    mInitialHotelsFilter = arguments.getParcelable(FilterActivity.EXTRA_FILTER);
    mInitialFilterParams = arguments.getParcelable(FilterActivity.EXTRA_FILTER_PARAMS);
  }

  private boolean tryRecognizeHiddenCommand(@NonNull String query)
  {
    for(HiddenCommand command: getHiddenCommands())
    {
      if (command.execute(query))
        return true;
    }

    return false;
  }

  @NonNull
  private List<HiddenCommand> getHiddenCommands()
  {
    if (mHiddenCommands.isEmpty())
    {
      mHiddenCommands.addAll(Arrays.asList(new BadStorageCommand("?emulateBadStorage"),
                                           new JavaCrashCommand("?emulateJavaCrash"),
                                           new NativeCrashCommand("?emulateNativeCrash"),
                                           new PushTokenCommand("?pushToken")));
    }

    return mHiddenCommands;
  }

  private void processSelected(@NonNull SearchResult result)
  {
    if (RoutingController.get().isWaitingPoiPick())
    {
      SearchResult.Description description = result.description;
      String subtitle = description != null
                        ? Utils.getLocalizedFeatureType(getContext(), description.featureType)
                        : "";
      String title = TextUtils.isEmpty(result.name) ? subtitle : "";

      final MapObject point = MapObject.createMapObject(FeatureId.EMPTY, MapObject.SEARCH,
                                                        title, subtitle, result.lat, result.lon);
      RoutingController.get().onPoiSelected(point);
    }

    if (mFilterController != null)
      mFilterController.resetFilter();

    mToolbarController.deactivate();

    if (getActivity() instanceof SearchActivity)
      Utils.navigateToParent(getActivity());
  }

  void showSingleResultOnMap(@NonNull SearchResult result, int resultIndex)
  {
    final String query = getQuery();
    SearchRecents.add(query);
    SearchEngine.INSTANCE.cancel();

    if (!RoutingController.get().isWaitingPoiPick())
      SearchEngine.INSTANCE.showResult(resultIndex);

    processSelected(result);

    Statistics.INSTANCE.trackEvent(Statistics.EventName.SEARCH_ITEM_CLICKED);
  }

  void showAllResultsOnMap()
  {
    // The previous search should be cancelled before the new one is started, since previous search
    // results are no longer needed.
    SearchEngine.INSTANCE.cancel();

    final String query = getQuery();
    SearchRecents.add(query);
    mLastQueryTimestamp = System.nanoTime();

    HotelsFilter hotelsFilter = null;
    BookingFilterParams bookingFilterParams = null;
    if (mFilterController != null)
    {
      hotelsFilter = mFilterController.getFilter();
      bookingFilterParams = mFilterController.getBookingFilterParams();
    }

    SearchEngine.INSTANCE.searchInteractive(
        query, !TextUtils.isEmpty(mInitialLocale)
               ? mInitialLocale : com.mapswithme.util.Language.getKeyboardLocale(),
        mLastQueryTimestamp, false /* isMapAndTable */,
        hotelsFilter, bookingFilterParams);
    SearchEngine.INSTANCE.setQuery(query);
    Utils.navigateToParent(getActivity());

    Statistics.INSTANCE.trackEvent(Statistics.EventName.SEARCH_ON_MAP_CLICKED);
  }

  private void onSearchEnd()
  {
    if (mSearchRunning && isAdded())
      updateSearchView();
  }

  private void updateSearchView()
  {
    mSearchRunning = false;
    mToolbarController.showProgress(false);
    updateFrames();
    updateResultsPlaceholder();
  }

  private void stopSearch()
  {
    SearchEngine.INSTANCE.cancel();
    updateSearchView();
  }

  private boolean isTabletSearch()
  {
    // TODO @yunitsky Implement more elegant solution.
    return getActivity() instanceof MwmActivity;
  }

  private void runSearch()
  {
    // The previous search should be cancelled before the new one is started, since previous search
    // results are no longer needed.
    SearchEngine.INSTANCE.cancel();

    HotelsFilter hotelsFilter = null;
    BookingFilterParams bookingFilterParams = null;
    if (mFilterController != null)
    {
      hotelsFilter = mFilterController.getFilter();
      bookingFilterParams = mFilterController.getBookingFilterParams();
    }

    mLastQueryTimestamp = System.nanoTime();
    if (isTabletSearch())
    {
      SearchEngine.INSTANCE.searchInteractive(getQuery(), mLastQueryTimestamp, true /* isMapAndTable */,
                                     hotelsFilter, bookingFilterParams);
    }
    else
    {
      if (!SearchEngine.INSTANCE.search(getQuery(), mLastQueryTimestamp, mLastPosition.valid,
                               mLastPosition.lat, mLastPosition.lon,
                               hotelsFilter, bookingFilterParams))
      {
        return;
      }
    }

    mSearchRunning = true;
    mToolbarController.showProgress(true);

    updateFrames();
  }

  @Override
  public void onResultsUpdate(@NonNull SearchResult[] results, long timestamp, boolean isHotel)
  {
    if (!isAdded() || !mToolbarController.hasQuery())
      return;

    refreshSearchResults(isHotel, results);
  }

  @Override
  public void onResultsEnd(long timestamp)
  {
    onSearchEnd();
  }

  @Override
  public void onFilterHotels(@BookingFilter.Type int type, @Nullable FeatureId[] hotels)
  {
    if (hotels == null)
      return;

    mSearchAdapter.setFilteredHotels(type, hotels);
  }

  @Override
  public void onSearchCategorySelected(@Nullable String category)
  {
    mToolbarController.setQuery(category);
  }

  @Override
  public void onPromoCategorySelected(@NonNull PromoCategory promo)
  {
    // Do nothing by default.
  }

  @Override
  public void onAdsRemovalSelected()
  {
    // Do nothing by default.
  }

  private void refreshSearchResults(boolean isHotel, @NonNull SearchResult[] results)
  {
    mSearchRunning = true;
    updateFrames();
    mSearchAdapter.refreshData(results);
    mToolbarController.showProgress(true);
    updateFilterButton(isHotel);
  }

  private void updateFilterButton(boolean isHotel)
  {
    if (mFilterController != null)
    {
      mFilterController.updateFilterButtonVisibility(isHotel);
      if (!isHotel)
        mFilterController.setFilterAndParams(null, null);
    }
  }

  @Override
  public void onActivityResult(int requestCode, int resultCode, Intent data)
  {
    super.onActivityResult(requestCode, resultCode, data);
    mToolbarController.onActivityResult(requestCode, resultCode, data);

    if (resultCode != Activity.RESULT_OK)
      return;

    switch (requestCode)
    {
      case FilterActivity.REQ_CODE_FILTER:
        if (data == null)
          break;

        if (mFilterController == null)
          return;

        mFilterController.setFilterAndParams(data.getParcelableExtra(FilterActivity.EXTRA_FILTER),
                                             data.getParcelableExtra(FilterActivity.EXTRA_FILTER_PARAMS));
        runSearch();
        break;
    }
  }

  @Override
  public boolean onBackPressed()
  {
    if (mToolbarController.hasQuery())
    {
      mToolbarController.clear();
      return true;
    }

    boolean isSearchActivity = getActivity() instanceof SearchActivity;
    mToolbarController.deactivate();
    if (RoutingController.get().isWaitingPoiPick())
    {
      RoutingController.get().onPoiSelected(null);
      if (isSearchActivity)
        closeSearch();
      return !isSearchActivity;
    }

    if (isSearchActivity)
      closeSearch();
    return isSearchActivity;
  }

  private void closeSearch()
  {
    getActivity().finish();
    getActivity().overridePendingTransition(android.R.anim.fade_in, android.R.anim.fade_out);
  }

  public void setRecyclerScrollListener(RecyclerView recycler)
  {
    recycler.addOnScrollListener(mRecyclerListener);
    mAttachedRecyclers.add(recycler);
  }

  @Override
  public SearchToolbarController getController()
  {
    return mToolbarController;
  }

  private static class BadStorageCommand extends HiddenCommand.BaseHiddenCommand
  {
    BadStorageCommand(@NonNull String command)
    {
      super(command);
    }

    @Override
    void executeInternal()
    {
      SharedPropertiesUtils.setShouldShowEmulateBadStorageSetting(true);
    }
  }

  private static class JavaCrashCommand extends HiddenCommand.BaseHiddenCommand
  {
    JavaCrashCommand(@NonNull String command)
    {
      super(command);
    }

    @Override
    void executeInternal()
    {
      throw new RuntimeException("Diagnostic java crash!");
    }
  }

  private static class NativeCrashCommand extends HiddenCommand.BaseHiddenCommand
  {
    NativeCrashCommand(@NonNull String command)
    {
      super(command);
    }

    @Override
    void executeInternal()
    {
      Framework.nativeMakeCrash();
    }
  }

  private static class PushTokenCommand extends HiddenCommand.BaseHiddenCommand
  {
    PushTokenCommand(@NonNull String command)
    {
      super(command);
    }

    @Override
    void executeInternal()
    {
      Logger logger = LoggerFactory.INSTANCE.getLogger(LoggerFactory.Type.THIRD_PARTY);
      String tag = PushTokenCommand.class.getSimpleName();
      logger.i(tag, "Push token: " + Pushwoosh.getInstance().getPushToken());
    }
  }
}
