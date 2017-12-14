package com.mapswithme.maps.discovery;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.os.Bundle;
import android.support.annotation.CallSuper;
import android.support.annotation.IdRes;
import android.support.annotation.MainThread;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import com.mapswithme.maps.R;
import com.mapswithme.maps.activity.CustomNavigateUpListener;
import com.mapswithme.maps.base.BaseMwmToolbarFragment;
import com.mapswithme.maps.bookmarks.data.FeatureId;
import com.mapswithme.maps.bookmarks.data.MapObject;
import com.mapswithme.maps.gallery.ItemSelectedListener;
import com.mapswithme.maps.gallery.Items;
import com.mapswithme.maps.gallery.impl.BaseItemSelectedListener;
import com.mapswithme.maps.gallery.impl.Factory;
import com.mapswithme.maps.search.SearchResult;
import com.mapswithme.maps.viator.ViatorProduct;
import com.mapswithme.maps.widget.PlaceholderView;
import com.mapswithme.maps.widget.ToolbarController;
import com.mapswithme.maps.widget.recycler.ItemDecoratorFactory;
import com.mapswithme.util.ConnectionState;
import com.mapswithme.util.Language;
import com.mapswithme.util.NetworkPolicy;
import com.mapswithme.util.UiUtils;
import com.mapswithme.util.Utils;
import com.mapswithme.util.statistics.GalleryType;
import com.mapswithme.util.statistics.Statistics;

import static com.mapswithme.util.statistics.Destination.EXTERNAL;
import static com.mapswithme.util.statistics.Destination.PLACEPAGE;
import static com.mapswithme.util.statistics.Destination.ROUTING;
import static com.mapswithme.util.statistics.GalleryPlacement.DISCOVERY;
import static com.mapswithme.util.statistics.GalleryType.LOCAL_EXPERTS;
import static com.mapswithme.util.statistics.GalleryType.SEARCH_ATTRACTIONS;
import static com.mapswithme.util.statistics.GalleryType.SEARCH_RESTAURANTS;
import static com.mapswithme.util.statistics.GalleryType.VIATOR;

public class DiscoveryFragment extends BaseMwmToolbarFragment implements UICallback
{
  private static final int ITEMS_COUNT = 5;
  private static final int[] ITEM_TYPES = { DiscoveryParams.ITEM_TYPE_VIATOR,
                                            DiscoveryParams.ITEM_TYPE_ATTRACTIONS,
                                            DiscoveryParams.ITEM_TYPE_CAFES,
                                            DiscoveryParams.ITEM_TYPE_LOCAL_EXPERTS };
  @Nullable
  private BaseItemSelectedListener<Items.Item> mDefaultListener;
  private boolean mOnlineMode;
  @Nullable
  private CustomNavigateUpListener mNavigateUpListener;
  @Nullable
  private DiscoveryListener mDiscoveryListener;
  @NonNull
  private final BroadcastReceiver mNetworkStateReceiver = new BroadcastReceiver()
  {
    @Override
    public void onReceive(Context context, Intent intent)
    {
      if (mOnlineMode)
        return;

      if (ConnectionState.isConnected())
        requestDiscoveryInfoAndInitAdapters();
    }
  };

  @Override
  public void onAttach(Context context)
  {
    super.onAttach(context);

    if (context instanceof CustomNavigateUpListener)
      mNavigateUpListener = (CustomNavigateUpListener) context;

    if (context instanceof DiscoveryListener)
      mDiscoveryListener = (DiscoveryListener) context;
  }

  @Override
  public void onDetach()
  {
    super.onDetach();
    mNavigateUpListener = null;
    mDiscoveryListener = null;
  }

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container,
                           @Nullable Bundle savedInstanceState)
  {
    return inflater.inflate(R.layout.fragment_discovery, container, false);
  }

  private void initViatorGallery()
  {
    setLayoutManagerAndItemDecoration(getContext(), getGallery(R.id.thingsToDo));
  }

  private void initAttractionsGallery()
  {
    setLayoutManagerAndItemDecoration(getContext(), getGallery(R.id.attractions));
  }

  private void initFoodGallery()
  {
    setLayoutManagerAndItemDecoration(getContext(), getGallery(R.id.food));
  }

  private void initLocalExpertsGallery()
  {
    setLayoutManagerAndItemDecoration(getContext(), getGallery(R.id.localGuides));
  }

  private static void setLayoutManagerAndItemDecoration(@NonNull Context context,
                                                        @NonNull RecyclerView rv)
  {
    rv.setLayoutManager(new LinearLayoutManager(context, LinearLayoutManager.HORIZONTAL,
                                                false));
    rv.addItemDecoration(
        ItemDecoratorFactory.createSponsoredGalleryDecorator(context,
                                                             LinearLayoutManager.HORIZONTAL));
  }

  @NonNull
  private RecyclerView getGallery(@IdRes int id)
  {
    View view = getView();
    if (view == null)
      throw new AssertionError("Root view is not initialized yet!");

    RecyclerView rv = (RecyclerView) view.findViewById(id);
    if (rv == null)
      throw new AssertionError("RecyclerView must be within root view!");
    return rv;
  }

  @Override
  public void onStart()
  {
    super.onStart();
    IntentFilter filter = new IntentFilter(ConnectivityManager.CONNECTIVITY_ACTION);
    getActivity().registerReceiver(mNetworkStateReceiver, filter);
    DiscoveryManager.INSTANCE.attach(this);
  }

  @Override
  public void onStop()
  {
    getActivity().unregisterReceiver(mNetworkStateReceiver);
    DiscoveryManager.INSTANCE.detach();
    super.onStop();
  }

  @Override
  public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState)
  {
    super.onViewCreated(view, savedInstanceState);
    mToolbarController.setTitle(R.string.discovery_button_title);
    mDefaultListener = new BaseItemSelectedListener<>(getActivity());
    view.findViewById(R.id.viatorLogo).setOnClickListener(new View.OnClickListener()
    {
      @Override
      public void onClick(View v)
      {
        Utils.openUrl(getActivity(), DiscoveryManager.nativeGetViatorUrl());
        Statistics.INSTANCE.trackGalleryEvent(Statistics.EventName.PP_SPONSOR_LOGO_SELECTED,
                                              VIATOR, DISCOVERY);
      }
    });
    initViatorGallery();
    initAttractionsGallery();
    initFoodGallery();
    initLocalExpertsGallery();
    initSearchBasedAdapters();
    requestDiscoveryInfoAndInitAdapters();
  }

  private void requestDiscoveryInfoAndInitAdapters()
  {
    NetworkPolicy.checkNetworkPolicy(getFragmentManager(), new NetworkPolicy.NetworkPolicyListener()
    {
      @Override
      public void onResult(@NonNull NetworkPolicy policy)
      {
        mOnlineMode = policy.сanUseNetwork();
        initNetworkBasedAdapters();
        requestDiscoveryInfo();
      }
    });
  }

  private void initSearchBasedAdapters()
  {
    getGallery(R.id.attractions).setAdapter(Factory.createSearchBasedLoadingAdapter());
    getGallery(R.id.food).setAdapter(Factory.createSearchBasedLoadingAdapter());
  }

  private void initNetworkBasedAdapters()
  {
    UiUtils.showIf(mOnlineMode, getRootView(), R.id.localGuidesTitle, R.id.localGuides);
    if (mOnlineMode)
    {
      RecyclerView thinsToDo = getGallery(R.id.thingsToDo);
      thinsToDo.setAdapter(Factory.createViatorLoadingAdapter(DiscoveryManager.nativeGetViatorUrl(),
                                                              mDefaultListener));

      RecyclerView localGuides = getGallery(R.id.localGuides);
      localGuides.setAdapter(Factory.createLocalExpertsLoadingAdapter());
      return;
    }

    // It means that the user doesn't permit mobile network usage, so network based galleries UI
    // should be hidden in this case.
    if (ConnectionState.isMobileConnected())
    {
      UiUtils.hide(getView(), R.id.thingsToDoLayout, R.id.thingsToDo,
                   R.id.localGuidesTitle, R.id.localGuides);
    }
    else
    {
      UiUtils.show(getView(), R.id.thingsToDoLayout, R.id.thingsToDo);
      RecyclerView thinsToDo = getGallery(R.id.thingsToDo);
      thinsToDo.setAdapter(Factory.createViatorOfflineAdapter(new ViatorOfflineSelectedListener
                                                                  (getActivity()), DISCOVERY));
    }
  }

  private void requestDiscoveryInfo()
  {
    DiscoveryParams params;
    if (mOnlineMode)
    {
      params = new DiscoveryParams(Utils.getCurrencyCode(), Language.getDefaultLocale(),
                                   ITEMS_COUNT, ITEM_TYPES);
    }
    else
    {
      params = new DiscoveryParams(Utils.getCurrencyCode(), Language.getDefaultLocale(),
                                   ITEMS_COUNT, DiscoveryParams.ITEM_TYPE_ATTRACTIONS,
                                   DiscoveryParams.ITEM_TYPE_CAFES);
    }
    DiscoveryManager.INSTANCE.discover(params);
  }

  @MainThread
  @Override
  public void onAttractionsReceived(@NonNull SearchResult[] results)
  {
    updateViewsVisibility(results, R.id.attractionsTitle, R.id.attractions);
    ItemSelectedListener<Items.SearchItem> listener
        = createSearchProductItemListener(SEARCH_ATTRACTIONS);
    getGallery(R.id.attractions).setAdapter(Factory.createSearchBasedAdapter(results, listener,
                                                                             SEARCH_ATTRACTIONS,
                                                                             DISCOVERY));
  }

  @MainThread
  @Override
  public void onCafesReceived(@NonNull SearchResult[] results)
  {
    updateViewsVisibility(results, R.id.eatAndDrinkTitle, R.id.food);
    ItemSelectedListener<Items.SearchItem> listener =
        createSearchProductItemListener(SEARCH_RESTAURANTS);
    getGallery(R.id.food).setAdapter(Factory.createSearchBasedAdapter(results, listener,
                                                                      SEARCH_RESTAURANTS,
                                                                      DISCOVERY));
  }

  @MainThread
  @Override
  public void onViatorProductsReceived(@NonNull ViatorProduct[] products)
  {
    updateViewsVisibility(products, R.id.thingsToDoLayout, R.id.thingsToDo);
    String url = DiscoveryManager.nativeGetViatorUrl();
    ItemSelectedListener<Items.ViatorItem> listener
        = createOnlineProductItemListener(VIATOR);
    getGallery(R.id.thingsToDo).setAdapter(Factory.createViatorAdapter(products, url, listener,
                                                                       DISCOVERY));
  }

  @MainThread
  @Override
  public void onLocalExpertsReceived(@NonNull LocalExpert[] experts)
  {
    updateViewsVisibility(experts, R.id.localGuidesTitle, R.id.localGuides);
    String url = DiscoveryManager.nativeGetLocalExpertsUrl();
    ItemSelectedListener<Items.LocalExpertItem> listener
        = createOnlineProductItemListener(LOCAL_EXPERTS);
    getGallery(R.id.localGuides).setAdapter(Factory.createLocalExpertsAdapter(experts, url, listener,
                                                                              DISCOVERY));
  }

  @Override
  public void onError(@NonNull ItemType type)
  {
    switch (type)
    {
      case VIATOR:
        String url = DiscoveryManager.nativeGetViatorUrl();
        getGallery(R.id.thingsToDo).setAdapter(Factory.createViatorErrorAdapter(url, mDefaultListener));
        Statistics.INSTANCE.trackGalleryError(VIATOR, DISCOVERY, null);
        break;
      case ATTRACTIONS:
        getGallery(R.id.attractions).setAdapter(Factory.createSearchBasedErrorAdapter());
        Statistics.INSTANCE.trackGalleryError(SEARCH_ATTRACTIONS, DISCOVERY, null);
        break;
      case CAFES:
        getGallery(R.id.food).setAdapter(Factory.createSearchBasedErrorAdapter());
        Statistics.INSTANCE.trackGalleryError(SEARCH_RESTAURANTS, DISCOVERY, null);
        break;
      case LOCAL_EXPERTS:
        getGallery(R.id.localGuides).setAdapter(Factory.createLocalExpertsErrorAdapter());
        Statistics.INSTANCE.trackGalleryError(LOCAL_EXPERTS, DISCOVERY, null);
        break;
      default:
        throw new AssertionError("Unknown item type: " + type);
    }
  }

  @Override
  public void onNotFound()
  {
    View view = getRootView();

    UiUtils.hide(view, R.id.galleriesLayout);

    PlaceholderView placeholder = (PlaceholderView) view.findViewById(R.id.placeholder);
    placeholder.setContent(R.drawable.img_cactus, R.string.discovery_button_404_error_title,
                           R.string.discovery_button_404_error_message);
    UiUtils.show(placeholder);
  }

  private <T> void updateViewsVisibility(@NonNull T[] results, @IdRes int... viewsId)
  {
    for (@IdRes int id : viewsId)
      UiUtils.showIf(results.length != 0, getRootView().findViewById(id));
  }

  @NonNull
  public View getRootView()
  {
    View view = getView();
    if (view == null)
      throw new AssertionError("Don't call getRootView when view is not created yet!");
    return view;
  }

  private void routeTo(@NonNull Items.SearchItem item)
  {
    if (mDiscoveryListener == null)
      return;

    mDiscoveryListener.onRouteToDiscoveredObject(createMapObject(item));
  }

  private void showOnMap(@NonNull Items.SearchItem item)
  {
    if (mDiscoveryListener != null)
      mDiscoveryListener.onShowDiscoveredObject(createMapObject(item));
  }

  @NonNull
  private static MapObject createMapObject(@NonNull Items.SearchItem item)
  {
    String title = TextUtils.isEmpty(item.getTitle()) ? "" : item.getTitle();
    String subtitle = TextUtils.isEmpty(item.getSubtitle()) ? "" : item.getSubtitle();
    return MapObject.createMapObject(FeatureId.EMPTY, MapObject.SEARCH, title, subtitle,
                                     item.getLat(), item.getLon());
  }

  @Override
  protected ToolbarController onCreateToolbarController(@NonNull View root)
  {
    return new ToolbarController(getRootView(), getActivity())
    {
      @Override
      public void onUpClick()
      {
        if (mNavigateUpListener == null)
          return;

        mNavigateUpListener.customOnNavigateUp();
      }
    };
  }

  private <Item extends Items.Item> ItemSelectedListener<Item> createOnlineProductItemListener
      (final @NonNull GalleryType type)
  {
    return new BaseItemSelectedListener<Item>(getActivity())
    {
      @Override
      public void onItemSelected(@NonNull Item item, int position)
      {
        super.onItemSelected(item, position);
        Statistics.INSTANCE.trackGalleryProductItemSelected(type, DISCOVERY, position, EXTERNAL);
      }

      @Override
      public void onMoreItemSelected(@NonNull Item item)
      {
        super.onMoreItemSelected(item);
        Statistics.INSTANCE.trackGalleryEvent(Statistics.EventName.PP_SPONSOR_MORE_SELECTED, type,
                                              DISCOVERY);
      }
    };
  }

  private ItemSelectedListener<Items.SearchItem> createSearchProductItemListener
      (final @NonNull GalleryType type)
  {
    return new SearchBasedListener(this)
    {
      @Override
      public void onItemSelected(@NonNull Items.SearchItem item, int position)
      {
        super.onItemSelected(item, position);
        Statistics.INSTANCE.trackGalleryProductItemSelected(type, DISCOVERY, position, PLACEPAGE);
      }

      @Override
      public void onActionButtonSelected(@NonNull Items.SearchItem item, int position)
      {
        super.onActionButtonSelected(item, position);
        Statistics.INSTANCE.trackGalleryProductItemSelected(type, DISCOVERY, position,
                                                            ROUTING);
      }
    };
  }

  private static class ViatorOfflineSelectedListener extends BaseItemSelectedListener<Items.Item>
  {
    private ViatorOfflineSelectedListener(@NonNull Activity context)
    {
      super(context);
    }

    @Override
    public void onActionButtonSelected(@NonNull Items.Item item, int position)
    {
      Utils.showSystemSettings(getContext());
    }
  }

  private static class SearchBasedListener extends BaseItemSelectedListener<Items.SearchItem>
  {
    @NonNull
    private final DiscoveryFragment mFragment;

    private SearchBasedListener(@NonNull DiscoveryFragment fragment)
    {
      super(fragment.getActivity());
      mFragment = fragment;
    }

    @Override
    @CallSuper
    public void onItemSelected(@NonNull Items.SearchItem item, int position)
    {
      mFragment.showOnMap(item);
    }

    @Override
    @CallSuper
    public void onActionButtonSelected(@NonNull Items.SearchItem item, int position)
    {
      mFragment.routeTo(item);
    }
  }

  public interface DiscoveryListener {
    void onRouteToDiscoveredObject(@NonNull MapObject object);
    void onShowDiscoveredObject(@NonNull MapObject object);
  }
}
