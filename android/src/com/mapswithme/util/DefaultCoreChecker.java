package com.mapswithme.util;

import android.content.Context;
import android.support.annotation.NonNull;

import com.mapswithme.maps.MwmApplication;

public class DefaultCoreChecker implements CoreInitChecker
{
  @Override
  public boolean check(@NonNull Context context)
  {
    MwmApplication app = (MwmApplication) context.getApplicationContext();
    return isCoreDependent() && app.arePlatformAndCoreInitialized();
  }

  @Override
  public boolean isCoreDependent()
  {
    return true;
  }
}
