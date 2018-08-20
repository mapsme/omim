package com.mapswithme.maps.background;

import android.app.Activity;
import android.app.Application;
import android.os.Bundle;
import android.util.SparseArray;

import java.lang.ref.WeakReference;

import com.mapswithme.maps.MwmApplication;
import com.mapswithme.util.Listeners;
import com.mapswithme.util.concurrency.UiThread;
import com.mapswithme.util.log.Logger;
import com.mapswithme.util.log.LoggerFactory;

/**
 * Helper class that detects when the application goes to background and back to foreground.
 * <br/>Must be created as early as possible, i.e. in Application.onCreate().
 */
public final class AppBackgroundTracker
{
  private static final Logger LOGGER = LoggerFactory.INSTANCE.getLogger(LoggerFactory.Type.MISC);
  private static final String TAG = AppBackgroundTracker.class.getSimpleName();
  private static final int TRANSITION_DELAY_MS = 1000;

  private final Listeners<OnTransitionListener> mTransitionListeners = new Listeners<>();
  private final Listeners<OnVisibleAppLaunchListener> mVisibleAppLaunchListeners = new Listeners<>();
  private SparseArray<WeakReference<Activity>> mActivities = new SparseArray<>();
  private volatile boolean mForeground;

  private final Runnable mTransitionProc = new Runnable()
  {
    @Override
    public void run()
    {
      SparseArray<WeakReference<Activity>> newArray = new SparseArray<>();
      for (int i = 0; i < mActivities.size(); i++)
      {
        int key = mActivities.keyAt(i);
        WeakReference<Activity> ref = mActivities.get(key);
        Activity activity = ref.get();
        if (activity != null && !activity.isFinishing())
          newArray.put(key, ref);
      }

      mActivities = newArray;
      boolean old = mForeground;
      mForeground = (mActivities.size() > 0);

      if (mForeground != old)
        notifyTransitionListeners();
    }
  };

  /** @noinspection FieldCanBeLocal*/
  private final Application.ActivityLifecycleCallbacks mAppLifecycleCallbacks = new Application.ActivityLifecycleCallbacks()
  {
    private void onActivityChanged()
    {
      UiThread.cancelDelayedTasks(mTransitionProc);
      UiThread.runLater(mTransitionProc, TRANSITION_DELAY_MS);
    }

    @Override
    public void onActivityStarted(Activity activity)
    {
      LOGGER.d(TAG, "onActivityStarted activity = " + activity);
      if (mActivities.size() == 0)
        notifyVisibleAppLaunchListeners();
      mActivities.put(activity.hashCode(), new WeakReference<>(activity));
      onActivityChanged();
    }

    @Override
    public void onActivityStopped(Activity activity)
    {
      LOGGER.d(TAG, "onActivityStopped activity = " + activity);
      mActivities.remove(activity.hashCode());
      onActivityChanged();
    }

    @Override
    public void onActivityCreated(Activity activity, Bundle savedInstanceState)
    {
      LOGGER.d(TAG, "onActivityCreated activity = " + activity);
    }

    @Override
    public void onActivityDestroyed(Activity activity)
    {
      LOGGER.d(TAG, "onActivityDestroyed activity = " + activity);
    }

    @Override
    public void onActivityResumed(Activity activity)
    {
      LOGGER.d(TAG, "onActivityResumed activity = " + activity);
    }

    @Override
    public void onActivityPaused(Activity activity)
    {
      LOGGER.d(TAG, "onActivityPaused activity = " + activity);
    }

    @Override
    public void onActivitySaveInstanceState(Activity activity, Bundle outState)
    {
      LOGGER.d(TAG, "onActivitySaveInstanceState activity = " + activity);
    }
  };

  public interface OnTransitionListener
  {
    void onTransit(boolean foreground);
  }

  public interface OnVisibleAppLaunchListener
  {
    void onVisibleAppLaunch();
  }

  public AppBackgroundTracker()
  {
    MwmApplication.get().registerActivityLifecycleCallbacks(mAppLifecycleCallbacks);
  }

  public boolean isForeground()
  {
    return mForeground;
  }

  private void notifyTransitionListeners()
  {
    for (OnTransitionListener listener : mTransitionListeners)
      listener.onTransit(mForeground);
    mTransitionListeners.finishIterate();
  }

  private void notifyVisibleAppLaunchListeners()
  {
    for (OnVisibleAppLaunchListener listener : mVisibleAppLaunchListeners)
      listener.onVisibleAppLaunch();
    mVisibleAppLaunchListeners.finishIterate();
  }

  public void addListener(OnTransitionListener listener)
  {
    mTransitionListeners.register(listener);
  }

  public void removeListener(OnTransitionListener listener)
  {
    mTransitionListeners.unregister(listener);
  }

  public void addListener(OnVisibleAppLaunchListener listener)
  {
    mVisibleAppLaunchListeners.register(listener);
  }

  public void removeListener(OnVisibleAppLaunchListener listener)
  {
    mVisibleAppLaunchListeners.unregister(listener);
  }

  @android.support.annotation.UiThread
  public Activity getTopActivity()
  {
    return (mActivities.size() == 0 ? null : mActivities.get(mActivities.keyAt(0)).get());
  }
}
