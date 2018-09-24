package com.mapswithme.util.sharing;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.StringRes;

import com.mapswithme.maps.Framework;
import com.mapswithme.maps.R;
import com.mapswithme.maps.bookmarks.data.MapObject;
import com.mapswithme.maps.widget.placepage.Sponsored;
import com.mapswithme.util.Utils;
import com.mapswithme.util.statistics.Statistics;

public abstract class ShareOption
{
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

  public void shareMapObject(Activity activity, @NonNull MapObject mapObject, @Nullable Sponsored sponsored)
  {
    SharingHelper.shareOutside(new MapObjectShareable(activity, mapObject, sponsored)
                 .setBaseIntent(new Intent(mBaseIntent)), mNameResId);
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
