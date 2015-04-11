package me.maps.mwmwear.widget;

import android.animation.Animator;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.TransitionDrawable;
import android.support.wearable.view.WearableListView;
import android.view.View;
import android.widget.LinearLayout;

import me.maps.mwmwear.R;

/**
 * Layout animates itself by scaling down-up and applying alpha when it's centered and uncentered in WearableListView.
 */
public class SelectableSearchLayout extends LinearLayout implements WearableListView.OnCenterProximityListener
{
  private static final int ANIMATION_MILLIS = 300;
  private static final float REDUCED_SIZE = 0.8f;
  private static final float REDUCED_ALPHA = 0.6f;

  private TransitionDrawable mTransitionBg;
  private boolean mIsCentered;

  SelectableSearchLayout(Context context)
  {
    this(context, R.layout.item_search_category, R.id.iv__category);
  }

  /**
   * @param context    context
   * @param layoutId   id of layout to merge inflate to the view
   * @param imageResId resource of image to apply bg transition
   */
  public SelectableSearchLayout(Context context, int layoutId, int imageResId)
  {
    super(context);
    View.inflate(context, layoutId, this);
    setScaleX(REDUCED_SIZE);
    setScaleY(REDUCED_SIZE);
    setAlpha(REDUCED_ALPHA);
    final Drawable drawable = findViewById(imageResId).getBackground();
    if (drawable != null && drawable instanceof TransitionDrawable)
      mTransitionBg = (TransitionDrawable) drawable;
  }

  @Override
  public void onCenterPosition(boolean animate)
  {
    if (mIsCentered)
      return;

    if (animate)
    {
      animate().scaleX(1).scaleY(1).alpha(1).setListener(new Animator.AnimatorListener()
      {
        @Override
        public void onAnimationStart(Animator animation)
        {
          if (mTransitionBg != null)
            mTransitionBg.startTransition(ANIMATION_MILLIS);
        }

        @Override
        public void onAnimationEnd(Animator animation)
        {

        }

        @Override
        public void onAnimationCancel(Animator animation)
        {

        }

        @Override
        public void onAnimationRepeat(Animator animation)
        {

        }
      });
    }
    else
    {
      setScaleX(1);
      setScaleY(1);
      setAlpha(1);
      if (mTransitionBg != null)
        mTransitionBg.startTransition(1);
    }
    mIsCentered = true;
  }

  @Override
  public void onNonCenterPosition(boolean animate)
  {
    if (!mIsCentered)
      return;

    if (animate)
    {
      animate().scaleX(REDUCED_SIZE).scaleY(REDUCED_SIZE).alpha(REDUCED_ALPHA).setListener(new Animator.AnimatorListener()
      {
        @Override
        public void onAnimationStart(Animator animation)
        {
          if (mTransitionBg != null)
            mTransitionBg.reverseTransition(ANIMATION_MILLIS);
        }

        @Override
        public void onAnimationEnd(Animator animation)
        {

        }

        @Override
        public void onAnimationCancel(Animator animation)
        {

        }

        @Override
        public void onAnimationRepeat(Animator animation)
        {

        }
      });
    }
    else
    {
      setScaleX(REDUCED_SIZE);
      setScaleY(REDUCED_SIZE);
      setAlpha(REDUCED_ALPHA);
      if (mTransitionBg != null)
        mTransitionBg.reverseTransition(0);
    }
    mIsCentered = false;
  }
}
