package com.mapswithme.maps.settings;


import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;
import com.mapswithme.maps.BuildConfig;
import com.mapswithme.maps.R;
import com.mapswithme.maps.WebContainerActivity;
import com.mapswithme.maps.base.BaseMwmFragment;
import com.mapswithme.util.Constants;
import com.mapswithme.util.UiUtils;
import com.mapswithme.util.Utils;
import com.mapswithme.util.statistics.AlohaHelper;
import com.mapswithme.util.statistics.Statistics;

public class AboutFragment extends BaseMwmFragment
                        implements View.OnClickListener
{
  @Override
  public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState)
  {
    View frame = inflater.inflate(R.layout.about, container, false);

    ((TextView)frame.findViewById(R.id.version))
      .setText(getString(R.string.version, BuildConfig.VERSION_NAME));

    frame.findViewById(R.id.web).setOnClickListener(this);
    frame.findViewById(R.id.blog).setOnClickListener(this);
    frame.findViewById(R.id.facebook).setOnClickListener(this);
    frame.findViewById(R.id.twitter).setOnClickListener(this);
    frame.findViewById(R.id.subscribe).setOnClickListener(this);
    frame.findViewById(R.id.rate).setOnClickListener(this);
    frame.findViewById(R.id.copyright).setOnClickListener(this);

    return frame;
  }

  @Override
  public void onClick(View v)
  {
    try
    {
      switch (v.getId())
      {
        case R.id.web:
          startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(Constants.Url.WEB_SITE)));
          break;

        case R.id.blog:
          startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(Constants.Url.WEB_BLOG)));
          break;

        case R.id.facebook:
          Statistics.INSTANCE.trackSimpleNamedEvent(Statistics.EventName.Settings.FACEBOOK);
          AlohaHelper.logClick(AlohaHelper.Settings.FACEBOOK);

          UiUtils.showFacebookPage(getActivity());
          break;

        case R.id.twitter:
          Statistics.INSTANCE.trackSimpleNamedEvent(Statistics.EventName.Settings.TWITTER);
          AlohaHelper.logClick(AlohaHelper.Settings.TWITTER);

          UiUtils.showTwitterPage(getActivity());
          break;

        case R.id.subscribe:
          Statistics.INSTANCE.trackSimpleNamedEvent(Statistics.EventName.Settings.SUBSCRIBE);
          AlohaHelper.logClick(AlohaHelper.Settings.MAIL_SUBSCRIBE);

          startActivity(new Intent(Intent.ACTION_SENDTO)
                          .setData(Utils.buildMailUri(Constants.Url.MAIL_MAPSME_SUBSCRIBE,
                                                      getString(R.string.subscribe_me_subject),
                                                      getString(R.string.subscribe_me_body))));
          break;

        case R.id.rate:
          Statistics.INSTANCE.trackSimpleNamedEvent(Statistics.EventName.Settings.RATE);
          AlohaHelper.logClick(AlohaHelper.Settings.RATE);

          UiUtils.openAppInMarket(getActivity(), BuildConfig.REVIEW_URL);
          break;

        case R.id.copyright:
          Statistics.INSTANCE.trackSimpleNamedEvent(Statistics.EventName.Settings.COPYRIGHT);
          AlohaHelper.logClick(AlohaHelper.Settings.COPYRIGHT);

          WebContainerActivity.show(getActivity(), Constants.Url.COPYRIGHT, R.string.copyright);
          break;
      }
    } catch (ActivityNotFoundException e)
    {
      AlohaHelper.logException(e);
    }
  }
}
