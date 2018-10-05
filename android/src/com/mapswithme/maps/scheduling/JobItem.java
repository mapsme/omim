package com.mapswithme.maps.scheduling;

import android.os.Build;
import android.support.annotation.NonNull;
import android.support.annotation.RequiresApi;

import com.firebase.jobdispatcher.JobService;
import com.mapswithme.util.Utils;

public enum JobItem
{
  CONNECTIVITY_CHANGED_ITEM
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
      };

  private static final int ID_BASIC = 1070;
  private static final int JOB_TYPE_SHIFTS = 12;
  private static final int ID_BASIS_INTENT_SERVICE = 500070;

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
  public abstract Class<?> getJobSchedulerServiceClass();


  @NonNull
  public abstract Class<? extends JobService> getFirebaseJobService();
}
