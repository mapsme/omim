package com.mapswithme.maps;

import android.animation.Animator;
import android.animation.ValueAnimator;
import android.os.Bundle;

import androidx.annotation.IntegerRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import android.view.View;
import android.view.animation.AccelerateInterpolator;

import com.mapswithme.util.Listeners;
import com.mapswithme.util.UiUtils;

class PanelAnimator
{
  private static final int WIDTH = UiUtils.dimen(R.dimen.panel_width);

  private final MwmActivity mActivity;
  private final Listeners<MwmActivity.LeftAnimationTrackListener> mAnimationTrackListeners = new Listeners<>();
  private final View mPanel;
  @IntegerRes
  private final int mDuration;

  PanelAnimator(MwmActivity activity)
  {
    mActivity = activity;
    mPanel = mActivity.findViewById(R.id.fragment_container);
    mDuration = mActivity.getResources().getInteger(R.integer.anim_panel);
  }

  void registerListener(@NonNull MwmActivity.LeftAnimationTrackListener animationTrackListener)
  {
    mAnimationTrackListeners.register(animationTrackListener);
  }

  private void track(ValueAnimator animation)
  {
    float offset = (Float) animation.getAnimatedValue();
    mPanel.setTranslationX(offset);
    mPanel.setAlpha(offset / WIDTH + 1.0f);

    for (MwmActivity.LeftAnimationTrackListener listener: mAnimationTrackListeners)
      listener.onTrackLeftAnimation(offset + WIDTH);
    mAnimationTrackListeners.finishIterate();
  }

  /** @param completionListener will be called before the fragment becomes actually visible */
  public void show(final Class<? extends Fragment> clazz, final Bundle args, @Nullable final Runnable completionListener)
  {
    if (isVisible())
    {
      if (mActivity.getFragment(clazz) != null)
      {
        if (completionListener != null)
          completionListener.run();
        return;
      }

      hide(new Runnable()
      {
        @Override
        public void run()
        {
          show(clazz, args, completionListener);
        }
      });

      return;
    }

    mActivity.replaceFragmentInternal(clazz, args);
    if (completionListener != null)
      completionListener.run();

    UiUtils.show(mPanel);

    for (MwmActivity.LeftAnimationTrackListener listener: mAnimationTrackListeners)
      listener.onTrackStarted(false);
    mAnimationTrackListeners.finishIterate();

    ValueAnimator animator = ValueAnimator.ofFloat(-WIDTH, 0.0f);
    animator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener()
    {
      @Override
      public void onAnimationUpdate(ValueAnimator animation)
      {
        track(animation);
      }
    });
    animator.addListener(new UiUtils.SimpleAnimatorListener()
    {
      @Override
      public void onAnimationEnd(Animator animation)
      {
        for (MwmActivity.LeftAnimationTrackListener listener: mAnimationTrackListeners)
          listener.onTrackStarted(true);
        mAnimationTrackListeners.finishIterate();

        mActivity.adjustCompass(UiUtils.getCompassYOffset(mActivity));
      }
    });

    animator.setDuration(mDuration);
    animator.setInterpolator(new AccelerateInterpolator());
    animator.start();
  }

  public void hide(@Nullable final Runnable completionListener)
  {
    if (!isVisible())
    {
      if (completionListener != null)
        completionListener.run();
      return;
    }

    for (MwmActivity.LeftAnimationTrackListener listener: mAnimationTrackListeners)
      listener.onTrackStarted(true);
    mAnimationTrackListeners.finishIterate();

    ValueAnimator animator = ValueAnimator.ofFloat(0.0f, -WIDTH);
    animator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener()
    {
      @Override
      public void onAnimationUpdate(ValueAnimator animation)
      {
        track(animation);
      }
    });
    animator.addListener(new UiUtils.SimpleAnimatorListener()
    {
      @Override
      public void onAnimationEnd(Animator animation)
      {
        UiUtils.hide(mPanel);

        for (MwmActivity.LeftAnimationTrackListener listener: mAnimationTrackListeners)
          listener.onTrackStarted(false);
        mAnimationTrackListeners.finishIterate();

        mActivity.adjustCompass(UiUtils.getCompassYOffset(mActivity));

        if (completionListener != null)
          completionListener.run();
      }
    });

    animator.setDuration(mDuration);
    animator.setInterpolator(new AccelerateInterpolator());
    animator.start();
  }

  public boolean isVisible()
  {
    return UiUtils.isVisible(mPanel);
  }
}
