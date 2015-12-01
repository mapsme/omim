package com.mapswithme.maps.background;

import android.app.IntentService;
import android.app.Notification;
import android.app.PendingIntent;
import android.content.Intent;
import android.support.annotation.Nullable;
import android.support.v4.app.NotificationCompat;
import android.support.v4.content.WakefulBroadcastReceiver;

import java.lang.ref.WeakReference;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.CountDownLatch;

import com.mapswithme.maps.MwmActivity;
import com.mapswithme.maps.MwmApplication;

public abstract class BaseKeepAliveService extends IntentService
{
  private static final Map<Class<? extends BaseKeepAliveService>, WeakReference<BaseKeepAliveService>> sAliveServices = new HashMap<>();

  private final CountDownLatch mWaitMonitor = new CountDownLatch(1);

  private static @Nullable BaseKeepAliveService getAliveService(Class<? extends BaseKeepAliveService> serviceClass)
  {
    WeakReference<BaseKeepAliveService> svc = sAliveServices.get(serviceClass);
    if (svc == null)
      return null;

    BaseKeepAliveService res = svc.get();
    if (res == null)
      sAliveServices.remove(serviceClass);

    return res;
  }

  public BaseKeepAliveService(String name)
  {
    super(name);
  }

  @Override
  protected final void onHandleIntent(Intent intent)
  {
    synchronized (sAliveServices)
    {
      BaseKeepAliveService svc = getAliveService(getClass());
      if (svc != null)
        return;

      sAliveServices.put(getClass(), new WeakReference<>(this));
    }

    startForeground(hashCode(), createNotification());
    try
    {
      mWaitMonitor.await();
    } catch (InterruptedException ignored) {}

    synchronized (sAliveServices)
    {
      sAliveServices.remove(getClass());
    }
    stopForeground(true);
    WakefulBroadcastReceiver.completeWakefulIntent(intent);
  }

  public static void start(Class<? extends BaseKeepAliveService> serviceClass)
  {
    synchronized (sAliveServices)
    {
      if (getAliveService(serviceClass) == null)
        WakefulBroadcastReceiver.startWakefulService(MwmApplication.get(), new Intent(MwmApplication.get(), serviceClass));
    }
  }

  public static void stop(Class<? extends BaseKeepAliveService> serviceClass)
  {
    synchronized (sAliveServices)
    {
      BaseKeepAliveService svc = getAliveService(serviceClass);
      if (svc != null)
        svc.mWaitMonitor.countDown();
    }
  }

  private Notification createNotification()
  {
    MwmApplication app = MwmApplication.get();
    Intent it = new Intent(app, MwmActivity.class)
                    .setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);

    NotificationCompat.Builder builder = new NotificationCompat.Builder(app)
                                             .setContentIntent(PendingIntent.getActivity(app, 0, it, 0))
                                             .setLocalOnly(true);
    setupNotification(builder);
    Notification res = builder.build();
    res.flags |= Notification.FLAG_NO_CLEAR;
    return res;
  }

  protected void setupNotification(NotificationCompat.Builder builder) {}
}
