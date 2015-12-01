package com.mapswithme.maps.background;

import android.support.v4.app.NotificationCompat;

import com.mapswithme.maps.MwmApplication;
import com.mapswithme.maps.R;

public class TrackRecordService extends BaseKeepAliveService
{
  public TrackRecordService()
  {
    super(TrackRecordService.class.getName());
  }

  @Override
  protected void setupNotification(NotificationCompat.Builder builder)
  {
    builder.setContentTitle(MwmApplication.get().getString(R.string.app_name))
           .setContentText(MwmApplication.get().getString(R.string.track_notification_text))
           .setSmallIcon(R.drawable.ic_notification)  // TODO: Use correct icon
           .setPriority(NotificationCompat.PRIORITY_MIN);
  }
}
