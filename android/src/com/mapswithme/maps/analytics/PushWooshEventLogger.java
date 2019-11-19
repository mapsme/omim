package com.mapswithme.maps.analytics;

import android.app.Application;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.mapswithme.maps.BuildConfig;
import com.mapswithme.util.UiUtils;
import com.mapswithme.util.log.Logger;
import com.mapswithme.util.log.LoggerFactory;
import com.mapswithme.util.statistics.PushwooshHelper;
import com.pushwoosh.Pushwoosh;
import com.pushwoosh.notification.PushwooshNotificationSettings;

class PushWooshEventLogger extends DefaultEventLogger
{
  private static final Logger LOGGER = LoggerFactory.INSTANCE.getLogger(LoggerFactory.Type.MISC);
  private static final String PW_EMPTY_APP_ID = "XXXXX";

  @Nullable
  private PushwooshHelper mPushwooshHelper;

  PushWooshEventLogger(@NonNull Application application)
  {
    super(application);
  }

  @Override
  public void initialize()
  {
    try
    {
      if (BuildConfig.PW_APPID.equals(PW_EMPTY_APP_ID))
        return;

      @ColorInt
      int color = UiUtils.getNotificationColor(getApplication());
      PushwooshNotificationSettings.setNotificationIconBackgroundColor(color);
      Pushwoosh pushManager = Pushwoosh.getInstance();
      pushManager.registerForPushNotifications();
      mPushwooshHelper = new PushwooshHelper();
    }
    catch(Exception e)
    {
      LOGGER.e("Pushwoosh", "Failed to init Pushwoosh", e);
    }
  }

  @Override
  public void sendTags(@NonNull String tag, @Nullable String[] params)
  {
    super.sendTags(tag, params);
    if (mPushwooshHelper != null)
      mPushwooshHelper.sendTags(tag, params);
  }
}
