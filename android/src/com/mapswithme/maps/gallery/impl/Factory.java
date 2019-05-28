package com.mapswithme.maps.gallery.impl;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import com.mapswithme.maps.discovery.LocalExpert;
import com.mapswithme.maps.gallery.GalleryAdapter;
import com.mapswithme.maps.gallery.ItemSelectedListener;
import com.mapswithme.maps.gallery.Items;
import com.mapswithme.maps.gallery.RegularAdapterStrategy;
import com.mapswithme.maps.search.SearchResult;
import com.mapswithme.util.statistics.GalleryPlacement;
import com.mapswithme.util.statistics.GalleryState;
import com.mapswithme.util.statistics.GalleryType;
import com.mapswithme.util.statistics.Statistics;

import java.util.Arrays;

import static com.mapswithme.util.statistics.GalleryState.OFFLINE;
import static com.mapswithme.util.statistics.GalleryState.ONLINE;
import static com.mapswithme.util.statistics.GalleryType.LOCAL_EXPERTS;

public class Factory
{
  @NonNull
  public static GalleryAdapter createSearchBasedAdapter(@NonNull SearchResult[] results,
                                                        @Nullable ItemSelectedListener<Items
                                                            .SearchItem> listener,
                                                        @NonNull GalleryType type,
                                                        @NonNull GalleryPlacement placement,
                                                        @Nullable Items.MoreSearchItem item)
  {
    trackProductGalleryShownOrError(results, type, OFFLINE, placement);
    return new GalleryAdapter<>(new SearchBasedAdapterStrategy(results, item), listener);
  }

  @NonNull
  public static GalleryAdapter createSearchBasedLoadingAdapter()
  {
    return new GalleryAdapter<>(new SimpleLoadingAdapterStrategy(), null);
  }

  @NonNull
  public static GalleryAdapter createSearchBasedErrorAdapter()
  {
    return new GalleryAdapter<>(new SimpleErrorAdapterStrategy(), null);
  }

  @NonNull
  public static GalleryAdapter createHotelAdapter(@NonNull SearchResult[] results,
                                                  @Nullable ItemSelectedListener<Items
                                                      .SearchItem> listener,
                                                  @NonNull GalleryType type,
                                                  @NonNull GalleryPlacement placement)
  {
    trackProductGalleryShownOrError(results, type, OFFLINE, placement);
    return new GalleryAdapter<>(new HotelAdapterStrategy(results), listener);
  }

  @NonNull
  public static GalleryAdapter createLocalExpertsAdapter(@NonNull LocalExpert[] experts,
                                                         @Nullable String expertsUrl,
                                                         @Nullable ItemSelectedListener<Items
                                                             .LocalExpertItem> listener,
                                                         @NonNull GalleryPlacement placement)
  {
    trackProductGalleryShownOrError(experts, LOCAL_EXPERTS, ONLINE, placement);
    return new GalleryAdapter<>(new LocalExpertsAdapterStrategy(experts, expertsUrl), listener);
  }

  @NonNull
  public static GalleryAdapter createLocalExpertsLoadingAdapter()
  {
    return new GalleryAdapter<>(new LocalExpertsLoadingAdapterStrategy(), null);
  }

  @NonNull
  public static GalleryAdapter createLocalExpertsErrorAdapter()
  {
    return new GalleryAdapter<>(new LocalExpertsErrorAdapterStrategy(), null);
  }

  @NonNull
  public static GalleryAdapter createCatalogPromoAdapter(@NonNull RegularAdapterStrategy.Item[] items,
                                                         @Nullable String url,
                                                         @Nullable ItemSelectedListener<RegularAdapterStrategy.Item> listener,
                                                         @NonNull GalleryPlacement placement)
  {
    Items.LocalExpertMoreItem item = new Items.LocalExpertMoreItem(url);
    CatalogPromoAdapterStrategy strategy = new CatalogPromoAdapterStrategy(Arrays.asList(items),
                                                                           item);
    return new GalleryAdapter<>(strategy, listener);
  }

  @NonNull
  public static GalleryAdapter createCatalogPromoLoadingAdapter()
  {
    return new GalleryAdapter<>(new CatalogPromoLoadingAdapterStrategy(), null);
  }

  @NonNull
  public static GalleryAdapter createCatalogPromoErrorAdapter()
  {
    return new GalleryAdapter<>(new CatalogPromoErrorAdapterStrategy(), null);
  }

  private static <Product> void trackProductGalleryShownOrError(@NonNull Product[] products,
                                                                @NonNull GalleryType type,
                                                                @NonNull GalleryState state,
                                                                @NonNull GalleryPlacement placement)
  {
    if (products.length == 0)
      Statistics.INSTANCE.trackGalleryError(type, placement, Statistics.ParamValue.NO_PRODUCTS);
    else
      Statistics.INSTANCE.trackGalleryShown(type, state, placement);
  }
}
