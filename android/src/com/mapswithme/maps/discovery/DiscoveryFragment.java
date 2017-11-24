package com.mapswithme.maps.discovery;

import android.os.Bundle;
import android.support.annotation.MainThread;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import com.mapswithme.maps.R;
import com.mapswithme.maps.base.BaseMwmToolbarFragment;
import com.mapswithme.maps.search.SearchResult;
import com.mapswithme.maps.viator.ViatorProduct;
import com.mapswithme.util.Language;
import com.mapswithme.util.Utils;

public class DiscoveryFragment extends BaseMwmToolbarFragment implements UICallback
{
  private static final int ITEMS_COUNT = 5;
  private static final int[] ITEM_TYPES = { DiscoveryParams.ITEM_TYPE_VIATOR,
                                            DiscoveryParams.ITEM_TYPE_ATTRACTIONS,
                                            DiscoveryParams.ITEM_TYPE_CAFES,
                                            DiscoveryParams.ITEM_TYPE_LOCAL_EXPERTS };
  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState)
  {
    return inflater.inflate(R.layout.fragment_discovery, container, false);
  }

  @Override
  public void onStart()
  {
    super.onStart();
    DiscoveryManager.INSTANCE.attach(this);
  }

  @Override
  public void onStop()
  {
    DiscoveryManager.INSTANCE.detach();
    super.onStop();
  }

  @Override
  public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState)
  {
    super.onViewCreated(view, savedInstanceState);
    mToolbarController.setTitle(R.string.discover_button_title);
    DiscoveryParams params = new DiscoveryParams(Utils.getCurrencyCode(), Language.getDefaultLocale(),
                                                 ITEMS_COUNT, ITEM_TYPES);
    DiscoveryManager.INSTANCE.discover(params);
  }

  @MainThread
  @Override
  public void onAttractionsReceived(@Nullable SearchResult[] results)
  {

  }

  @MainThread
  @Override
  public void onCafesReceived(@Nullable SearchResult[] results)
  {

  }

  @MainThread
  @Override
  public void onViatorProductsReceived(@Nullable ViatorProduct[] products)
  {

  }

  @MainThread
  @Override
  public void onLocalExpertsReceived(@Nullable LocalExpert[] experts)
  {

  }
}
