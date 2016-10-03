package com.mapswithme.util.sharing;

import android.app.Activity;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.text.TextUtils;

import com.mapswithme.maps.Framework;
import com.mapswithme.maps.R;
import com.mapswithme.maps.bookmarks.data.MapObject;
import com.mapswithme.maps.widget.placepage.SponsoredHotel;
import com.mapswithme.util.statistics.Statistics;

class MapObjectShareable extends BaseShareable
{
  MapObjectShareable(Activity context, @NonNull MapObject mapObject, @Nullable SponsoredHotel sponsoredHotel)
  {
    super(context);

    final Activity activity = getActivity();
    final double lat = mapObject.getLat();
    final double lon = mapObject.getLon();
    final double scale = mapObject.getScale();
    final String title = mapObject.getTitle();
    final String httpUrl = Framework.getHttpGe0Url(lat, lon, scale, title);
    final String ge0Url = Framework.nativeGetGe0Url(lat, lon, scale, title);

    final String subject;
    String text;
    if (MapObject.isOfType(MapObject.MY_POSITION, mapObject))
    {
      subject = activity.getString(R.string.my_position_share_email_subject);
      text = activity.getString(R.string.my_position_share_email,
                                Framework.nativeGetNameAndAddress(lat, lon), ge0Url, httpUrl);
    }
    else
    {
      subject = activity.getString(R.string.bookmark_share_email_subject);

      //TODO: sharing message will be redesigned according this task - https://jira.mail.ru/browse/MAPSME-1350
      text = lineWithBreak(activity.getString(R.string.sharing_call_action_look)) +
                 lineWithBreak(title) +
                 lineWithBreak(mapObject.getSubtitle()) +
                 lineWithBreak(mapObject.getAddress()) +
                 lineWithBreak(httpUrl) +
                 lineWithBreak(ge0Url);

      if (sponsoredHotel != null)
      {
        text += lineWithBreak(activity.getString(R.string.sharing_booking)) +
                    sponsoredHotel.getUrlBook();
      }
    }

    setSubject(subject);
    setText(text);
  }

  private String lineWithBreak(String title)
  {
    if (!TextUtils.isEmpty(title))
      return title + "\n";

    return "";
  }

  @Override
  public void share(SharingTarget target)
  {
    super.share(target);
    Statistics.INSTANCE.trackPlaceShared(target.name);
  }

  @Override
  public String getMimeType()
  {
    return null;
  }
}
