package com.mapswithme.util.statistics;

import android.app.Activity;
import android.graphics.Rect;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.view.View;

import com.mapswithme.maps.R;
import com.mapswithme.maps.bookmarks.data.MapObject;
import com.mapswithme.maps.taxi.TaxiManager;
import com.mapswithme.maps.widget.placepage.PlacePageView;
import com.mapswithme.maps.widget.placepage.Sponsored;
import com.mapswithme.util.UiUtils;

import java.util.List;

public class PlacePageTracker
{
  private final int mBottomPadding;
  @NonNull
  private final PlacePageView mPlacePageView;
  @NonNull
  private final View mTaxi;
  @NonNull
  private final View mViator;
  @Nullable
  private MapObject mMapObject;

  private boolean mTaxiTracked;
  private boolean mViatorTracked;

  public PlacePageTracker(@NonNull PlacePageView placePageView)
  {
    mPlacePageView = placePageView;
    mTaxi = mPlacePageView.findViewById(R.id.ll__place_page_taxi);
    mViator = mPlacePageView.findViewById(R.id.ll__place_viator);
    Activity activity = (Activity) placePageView.getContext();
    mBottomPadding = activity.getResources().getDimensionPixelOffset(R.dimen.place_page_buttons_height);
  }

  public void setMapObject(@Nullable MapObject mapObject)
  {
    mMapObject = mapObject;
  }

  public void onMove()
  {
    trackTaxiVisibility();
    trackViatorVisibility();
  }

  public void onHidden()
  {
    mTaxiTracked = false;
    mViatorTracked = false;
  }

  public void onOpened()
  {
    if (mPlacePageView.getState() == PlacePageView.State.DETAILS)
    {
      Sponsored sponsored = mPlacePageView.getSponsored();
      if (sponsored != null)
        Statistics.INSTANCE.trackSponsoredOpenEvent(sponsored.getType());
    }
  }

  private void trackTaxiVisibility()
  {
    if (!mTaxiTracked && isViewOnScreen(mTaxi) && mMapObject != null)
    {
      List<Integer> taxiTypes = mMapObject.getReachableByTaxiTypes();
      if (taxiTypes != null && !taxiTypes.isEmpty())
      {
        @TaxiManager.TaxiType
        int type = taxiTypes.get(0);
        Statistics.INSTANCE.trackTaxiEvent(Statistics.EventName.ROUTING_TAXI_REAL_SHOW_IN_PP, type);
        mTaxiTracked = true;
      }
    }
  }

  private void trackViatorVisibility()
  {
    if (!mViatorTracked && isViewOnScreen(mViator) && mPlacePageView.getSponsored() != null)
    {
      Sponsored sponsored = mPlacePageView.getSponsored();
      Statistics.INSTANCE.trackSponsoredGalleryShown(sponsored.getType());
      mViatorTracked = true;
    }
  }

  private boolean isViewOnScreen(@NonNull View view) {

    if (UiUtils.isInvisible(mPlacePageView) || UiUtils.isInvisible(view) || !view.isShown())
      return false;

    Rect localRect = new Rect();
    Rect globalRect = new Rect();
    view.getLocalVisibleRect(localRect);
    view.getGlobalVisibleRect(globalRect);

    return UiUtils.isLandscape(view.getContext()) ? isPartiallyVisible(view, localRect, globalRect)
                              : isCompletelyVisible(view, localRect, globalRect);
  }

  private boolean isCompletelyVisible(@NonNull View view, @NonNull Rect localRect,
                                      @NonNull Rect globalRect)
  {
    return view.getHeight() > 0 && localRect.bottom >= view.getHeight() && localRect.top == 0
           && globalRect.bottom <= mPlacePageView.getBottom() - mBottomPadding;
  }

  private boolean isPartiallyVisible(@NonNull View view, @NonNull Rect localRect,
                                     @NonNull Rect globalRect)
  {
    return view.getHeight() > 0 && localRect.top == 0 || globalRect.bottom < mPlacePageView.getBottom();
  }
}
