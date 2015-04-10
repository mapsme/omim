package me.maps.mwmwear;

import android.animation.Animator;
import android.content.Context;
import android.graphics.drawable.TransitionDrawable;
import android.support.wearable.view.WearableListView;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;

public class SelectableSearchLayout extends LinearLayout implements WearableListView.OnCenterProximityListener
{
  private static final int ANIMATION_MILLIS = 300;
  private final TransitionDrawable mTransitionBg;

  private static final float REDUCED_SIZE = 0.8f;
  private static final float REDUCED_ALPHA = 0.6f;
  private boolean mIsCentered;

  public SelectableSearchLayout(Context context)
  {
    this(context, null, 0);
  }

  public SelectableSearchLayout(Context context, AttributeSet attrs)
  {
    this(context, attrs, 0);
  }

  public SelectableSearchLayout(Context context, AttributeSet attrs, int defStyleAttr)
  {
    super(context, attrs, defStyleAttr);
    View.inflate(context, R.layout.item_search_category, this);
    setScaleX(REDUCED_SIZE);
    setScaleY(REDUCED_SIZE);
    setAlpha(REDUCED_ALPHA);
    mTransitionBg = (TransitionDrawable) findViewById(R.id.iv__category).getBackground();
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
      mTransitionBg.reverseTransition(0);
    }
    mIsCentered = false;
  }
}
