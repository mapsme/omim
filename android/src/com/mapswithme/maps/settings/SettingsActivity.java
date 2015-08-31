package com.mapswithme.maps.settings;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.preference.PreferenceActivity;
import android.support.annotation.NonNull;
import android.view.MenuItem;
import com.mapswithme.maps.BuildConfig;
import com.mapswithme.maps.MwmApplication;
import com.mapswithme.maps.R;
import com.mapswithme.maps.WebContainerActivity;
import com.mapswithme.util.Constants;
import com.mapswithme.util.Utils;
import com.mapswithme.util.ViewServer;
import com.mapswithme.util.statistics.AlohaHelper;
import com.mapswithme.util.statistics.Statistics;

import java.util.List;

public class SettingsActivity extends PreferenceActivity
{
  @Override
  public void onBuildHeaders(List<Header> target)
  {
    loadHeadersFromResource(R.xml.prefs_headers, target);
  }

  @Override
  public void onHeaderClick(@NonNull Header header, int position)
  {
    if (header.id == R.id.group_map)
      Statistics.INSTANCE.trackSimpleNamedEvent(Statistics.EventName.Settings.GROUP_MAP);
    else

    if (header.id == R.id.group_misc)
      Statistics.INSTANCE.trackSimpleNamedEvent(Statistics.EventName.Settings.GROUP_MISC);
    else

    if (header.id == R.id.help)
    {
      Statistics.INSTANCE.trackSimpleNamedEvent(Statistics.EventName.Settings.HELP);
      AlohaHelper.logClick(AlohaHelper.Settings.HELP);

      WebContainerActivity.show(this, Constants.Url.FAQ, R.string.help);
      return;
    }
    else

    if (header.id == R.id.feedback)
    {
      Statistics.INSTANCE.trackSimpleNamedEvent(Statistics.EventName.Settings.CONTACT_US);
      AlohaHelper.logClick(AlohaHelper.Settings.CONTACT_US);

      startActivity(new Intent(Intent.ACTION_SENDTO)
                        .setData(Utils.buildMailUri(Constants.Url.MAIL_MAPSME_INFO, "", "")));
      return;
    }
    else

    if (header.id == R.id.report_bug)
    {
      Statistics.INSTANCE.trackSimpleNamedEvent(Statistics.EventName.Settings.REPORT_BUG);
      AlohaHelper.logClick(AlohaHelper.Settings.REPORT_BUG);

      startActivity(new Intent(Intent.ACTION_SEND)
                        .setFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                        .putExtra(Intent.EXTRA_EMAIL, new String[] { BuildConfig.SUPPORT_MAIL })
                        .putExtra(Intent.EXTRA_SUBJECT, "[android] Bugreport from user")
                        .putExtra(Intent.EXTRA_STREAM, Uri.parse("file://" + Utils.saveLogToFile()))
                        .putExtra(Intent.EXTRA_TEXT, "")  // Some e-mail clients do complain about empty body
                        .setType("message/rfc822"));
      return;
    }
    else

    if (header.id == R.id.about)
    {
      Statistics.INSTANCE.trackSimpleNamedEvent(Statistics.EventName.Settings.ABOUT);
      AlohaHelper.logClick(AlohaHelper.Settings.ABOUT);

      startActivity(new Intent(this, AboutActivity.class));
      return;
    }

    super.onHeaderClick(header, position);
  }

  @Override
  protected void onCreate(Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);

    MwmApplication.get().initCounters();
    MwmApplication.get().initNativeCore();
    ViewServer.get(this).addWindow(this);
  }

  @Override
  protected void onDestroy()
  {
    super.onDestroy();

    ViewServer.get(this).removeWindow(this);
  }

  @Override
  protected boolean isValidFragment(String fragmentName)
  {
    return true;
  }

  @Override
  protected void onStart()
  {
    super.onStart();
    Statistics.INSTANCE.startActivity(this);
  }

  @Override
  protected void onStop()
  {
    super.onStop();
    Statistics.INSTANCE.stopActivity(this);
  }

  @Override
  protected void onResume()
  {
    super.onResume();

    org.alohalytics.Statistics.logEvent("$onResume", this.getClass().getSimpleName());
    ViewServer.get(this).setFocusedWindow(this);
  }

  @Override
  protected void onPause()
  {
    super.onPause();

    org.alohalytics.Statistics.logEvent("$onPause", this.getClass().getSimpleName());
  }

  @Override
  public boolean onOptionsItemSelected(MenuItem item)
  {
    if (item.getItemId() == android.R.id.home)
    {
      onBackPressed();
      return true;
    }

    return super.onOptionsItemSelected(item);
  }
}
