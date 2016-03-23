package com.mapswithme.util.sharing;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.support.annotation.StringRes;

import com.mapswithme.maps.Framework;
import com.mapswithme.maps.R;
import com.mapswithme.maps.bookmarks.data.MapObject;
import com.mapswithme.maps.bookmarks.data.MapObject.MapObjectType;
import com.mapswithme.util.Utils;
import com.mapswithme.util.statistics.Statistics;

public abstract class ShareOption
{
  public static final SmsShareOption SMS = new SmsShareOption();
  public static final EmailShareOption EMAIL = new EmailShareOption();
  public static final AnyShareOption ANY = new AnyShareOption();

  @StringRes
  protected final int mNameResId;
  protected final Intent mBaseIntent;

  protected ShareOption(int nameResId, Intent baseIntent)
  {
    mNameResId = nameResId;
    mBaseIntent = baseIntent;
  }

  public boolean isSupported(Context context)
  {
    return Utils.isIntentSupported(context, mBaseIntent);
  }

  public void shareMapObject(Activity activity, MapObject mapObject)
  {
    SharingHelper.shareOutside(new MapObjectShareable(activity, mapObject)
                 .setBaseIntent(new Intent(mBaseIntent)), mNameResId);
  }

  public static class SmsShareOption extends ShareOption
  {
    protected SmsShareOption()
    {
      super(R.string.share_by_message, new Intent(Intent.ACTION_VIEW));
    }

    private void shareWithText(Activity activity, String body)
    {
      Intent smsIntent = new Intent();
      TargetUtils.fillSmsIntent(activity, smsIntent, body);
      activity.startActivity(smsIntent);
      Statistics.INSTANCE.trackPlaceShared("SMS");
    }

    @Override
    public void shareMapObject(Activity activity, MapObject mapObject)
    {
      final String ge0Url = Framework.nativeGetGe0Url(mapObject.getLat(), mapObject.getLon(), mapObject.getScale(), "");
      final String httpUrl = Framework.getHttpGe0Url(mapObject.getLat(), mapObject.getLon(), mapObject.getScale(), "");
      final int bodyId = mapObject.getType() == MapObjectType.MY_POSITION ? R.string.my_position_share_sms : R.string.bookmark_share_sms;
      final String body = activity.getString(bodyId, ge0Url, httpUrl);

      shareWithText(activity, body);
    }
  }

  public static class EmailShareOption extends ShareOption
  {
    protected EmailShareOption()
    {
      super(R.string.share_by_email, new Intent(Intent.ACTION_SEND).setType(TargetUtils.TYPE_MESSAGE_RFC822));
    }
  }

  public static class AnyShareOption extends ShareOption
  {
    protected AnyShareOption()
    {
      super(R.string.share, new Intent(Intent.ACTION_SEND).setType(TargetUtils.TYPE_TEXT_PLAIN));
    }

    public void share(Activity activity, String body)
    {
      SharingHelper.shareOutside(new TextShareable(activity, body));
    }

    public void share(Activity activity, String body, @StringRes int titleRes)
    {
      SharingHelper.shareOutside(new TextShareable(activity, body), titleRes);
    }
  }
}