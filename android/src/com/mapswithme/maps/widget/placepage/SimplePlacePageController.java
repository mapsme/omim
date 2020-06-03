package com.mapswithme.maps.widget.placepage;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.Application;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.IdRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.view.GestureDetectorCompat;
import com.mapswithme.maps.Framework;
import com.mapswithme.maps.R;
import com.mapswithme.util.UiUtils;
import com.trafi.anchorbottomsheetbehavior.AnchorBottomSheetBehavior;

import java.util.Objects;

public class SimplePlacePageController implements PlacePageController
{
  @SuppressWarnings("NullableProblems")
  @NonNull
  private Application mApplication;
  @SuppressWarnings("NullableProblems")
  @NonNull
  private View mSheet;
  @SuppressWarnings("NullableProblems")
  @NonNull
  private AnchorBottomSheetBehavior<View> mSheetBehavior;
  @NonNull
  private final SlideListener mSlideListener;
  private int mViewportMinHeight;
  private int mViewPortMinWidth;
  @NonNull
  private final PlacePageViewRenderer<PlacePageData> mViewRenderer;
  @Nullable
  private final PlacePageStateObserver mStateObserver;
  @NonNull
  private final BottomSheetChangedListener mBottomSheetChangedListener =
      new BottomSheetChangedListener()
      {
        @Override
        public void onSheetHidden()
        {
          onHiddenInternal();
          if (mStateObserver != null)
            mStateObserver.onPlacePageClosed();
        }

        @Override
        public void onSheetDirectionIconChange()
        {
          if (UiUtils.isLandscape(mApplication))
            return;

          PlacePageUtils.setPullDrawable(mSheetBehavior, mSheet, R.id.pull_icon);
        }

        @Override
        public void onSheetDetailsOpened()
        {
          if (UiUtils.isLandscape(mApplication))
            PlacePageUtils.moveViewPortRight(mSheet, mViewPortMinWidth);
          if (mStateObserver != null)
            mStateObserver.onPlacePageDetails();
        }

        @Override
        public void onSheetCollapsed()
        {
          if (UiUtils.isLandscape(mApplication))
            PlacePageUtils.moveViewPortRight(mSheet, mViewPortMinWidth);
          if (mStateObserver != null)
            mStateObserver.onPlacePagePreview();
        }

        @Override
        public void onSheetSliding(int top)
        {
          if (UiUtils.isLandscape(mApplication))
            return;

          mSlideListener.onPlacePageSlide(top);
        }

        @Override
        public void onSheetSlideFinish()
        {
          if (UiUtils.isLandscape(mApplication))
            return;

          PlacePageUtils.moveViewportUp(mSheet, mViewportMinHeight);
        }
      };

  private final AnchorBottomSheetBehavior.BottomSheetCallback mSheetCallback
      = new DefaultBottomSheetCallback(mBottomSheetChangedListener);

  private boolean mDeactivateMapSelection = true;
  @IdRes
  private final int mSheetResId;

  SimplePlacePageController(int sheetResId, @NonNull PlacePageViewRenderer<PlacePageData> renderer,
                            @Nullable PlacePageStateObserver stateObserver,
                            @NonNull SlideListener slideListener)
  {
    mSheetResId = sheetResId;
    mSlideListener = slideListener;
    mViewRenderer = renderer;
    mStateObserver = stateObserver;
  }

  @Override
  public void openFor(@NonNull PlacePageData data)
  {
    mDeactivateMapSelection = true;
    mViewRenderer.render(data);
    if (mSheetBehavior.getSkipCollapsed())
      mSheetBehavior.setState(AnchorBottomSheetBehavior.STATE_EXPANDED);
    else
      mSheetBehavior.setState(AnchorBottomSheetBehavior.STATE_COLLAPSED);
  }

  @Override
  public void close(boolean deactivateMapSelection)
  {
    mDeactivateMapSelection = deactivateMapSelection;
    mSheetBehavior.setState(AnchorBottomSheetBehavior.STATE_HIDDEN);
  }

  @Override
  public boolean isClosed()
  {
    return PlacePageUtils.isHiddenState(mSheetBehavior.getState());
  }

  @Override
  public void onActivityCreated(Activity activity, Bundle savedInstanceState)
  {

  }

  @Override
  public void onActivityStarted(Activity activity)
  {

  }

  @Override
  public void onActivityResumed(Activity activity)
  {

  }

  @Override
  public void onActivityPaused(Activity activity)
  {

  }

  @Override
  public void onActivityStopped(Activity activity)
  {

  }

  @Override
  public void onActivitySaveInstanceState(Activity activity, Bundle outState)
  {

  }

  @Override
  public void onActivityDestroyed(Activity activity)
  {

  }

  @SuppressLint("ClickableViewAccessibility")
  @Override
  public void initialize(@Nullable Activity activity)
  {
    Objects.requireNonNull(activity);
    mApplication = activity.getApplication();
    mSheet = activity.findViewById(mSheetResId);
    mViewportMinHeight = mSheet.getResources().getDimensionPixelSize(R.dimen.viewport_min_height);
    mViewPortMinWidth = mSheet.getResources().getDimensionPixelSize(R.dimen.viewport_min_width);
    mSheetBehavior = AnchorBottomSheetBehavior.from(mSheet);
    mSheetBehavior.addBottomSheetCallback(mSheetCallback);
    boolean isLandscape = UiUtils.isLandscape(mApplication);
    GestureDetectorCompat gestureDetector = new GestureDetectorCompat(
        activity, new SimplePlacePageGestureListener(mSheetBehavior, isLandscape));
    mSheet.setOnTouchListener((v, event) -> gestureDetector.onTouchEvent(event));
    mViewRenderer.initialize(mSheet);
  }

  @Override
  public void destroy()
  {
    mViewRenderer.destroy();
  }

  @Override
  public void onSave(@NonNull Bundle outState)
  {
    mViewRenderer.onSave(outState);
  }

  @Override
  public void onRestore(@NonNull Bundle inState)
  {
    if (PlacePageUtils.isHiddenState(mSheetBehavior.getState()))
      return;

    if (!Framework.nativeHasPlacePageInfo())
    {
      close(false);
      return;
    }

    mViewRenderer.onRestore(inState);
    if (UiUtils.isLandscape(mApplication))
    {
      // In case when bottom sheet was collapsed for vertical orientation then after rotation
      // we should expand bottom sheet forcibly for horizontal orientation. It's by design.
      if (!PlacePageUtils.isHiddenState(mSheetBehavior.getState()))
      {
        mSheetBehavior.setState(AnchorBottomSheetBehavior.STATE_EXPANDED);
      }
      return;
    }

    PlacePageUtils.setPullDrawable(mSheetBehavior, mSheet, R.id.pull_icon);
  }

  private void onHiddenInternal()
  {
    mViewRenderer.onHide();
    if (mDeactivateMapSelection)
      Framework.nativeDeactivatePopup();
    mDeactivateMapSelection = true;
    if (UiUtils.isLandscape(mApplication))
    {
      PlacePageUtils.moveViewPortRight(mSheet, mViewPortMinWidth);
      return;
    }

    PlacePageUtils.moveViewportUp(mSheet, mViewportMinHeight);
  }

  @Override
  public boolean support(@NonNull PlacePageData data)
  {
    return mViewRenderer.support(data);
  }

  private static class SimplePlacePageGestureListener extends PlacePageGestureListener
  {
    private final boolean mLandscape;

    SimplePlacePageGestureListener(@NonNull AnchorBottomSheetBehavior<View> bottomSheetBehavior,
                                   boolean landscape)
    {
      super(bottomSheetBehavior);
      mLandscape = landscape;
    }

    @Override
    public boolean onSingleTapConfirmed(MotionEvent e)
    {
      if (mLandscape)
      {
        getBottomSheetBehavior().setState(AnchorBottomSheetBehavior.STATE_HIDDEN);
        return false;
      }

      return super.onSingleTapConfirmed(e);
    }
  }
}
