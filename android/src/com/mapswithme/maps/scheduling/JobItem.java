package com.mapswithme.maps.scheduling;

import android.os.Build;
import android.support.annotation.NonNull;
import android.support.annotation.RequiresApi;
import android.support.v4.app.JobIntentService;

import com.firebase.jobdispatcher.JobService;
import com.mapswithme.maps.background.NotificationService;
import com.mapswithme.maps.background.WorkerService;
import com.mapswithme.maps.bookmarks.SystemDownloadCompletedService;
import com.mapswithme.maps.location.TrackRecorderWakeService;
import com.mapswithme.util.Utils;

public enum JobItem
{
  CONNECTIVITY_CHANGED(NotificationService.class)
      {
        @NonNull
        @Override
        @RequiresApi(api = Build.VERSION_CODES.LOLLIPOP)
        public Class<?> getJobSchedulerServiceClass()
        {
          if (Utils.isLollipopOrLater())
              return NativeJobService.class;

          throw new UnsupportedOperationException("JobScheduler not supported before Lollipop api");
        }

        @NonNull
        @Override
        public Class<? extends JobService> getFirebaseJobService()
        {
          return FirebaseJobService.class;
        }
      },

  WORKER(WorkerService.class),
  TRACK_RECORDER(TrackRecorderWakeService.class),
  SYSTEM_DOWNLOAD(SystemDownloadCompletedService.class);

  private static final int ID_BASIC = 1070;
  private static final int JOB_TYPE_SHIFTS = 12;
  private static final int ID_BASIS_INTENT_SERVICE = 500070;

  @NonNull
  private final Class<? extends JobIntentService> mIntentServiceClass;

  JobItem(@NonNull Class<? extends JobIntentService> clazz)
  {
    mIntentServiceClass = clazz;
  }

  public int getId()
  {
    return calcIdentifier(ID_BASIC);
  }

  public int getIntentServiceId()
  {
    return calcIdentifier(ID_BASIS_INTENT_SERVICE);
  }

  private int calcIdentifier(int basis)
  {
    return (ordinal() + 1 << JOB_TYPE_SHIFTS) + basis;
  }

  @NonNull
  @RequiresApi(api = Build.VERSION_CODES.LOLLIPOP)
  public Class<?> getJobSchedulerServiceClass()
  {
    throw new UnsupportedOperationException("Not supported here! Value : " + name());
  }

  @NonNull
  public Class<? extends JobService> getFirebaseJobService()
  {
    throw new UnsupportedOperationException("Not supported here! Value : " + name());
  }

  @NonNull
  public Class<? extends JobIntentService> getIntentService()
  {
    return mIntentServiceClass;
  }
}
