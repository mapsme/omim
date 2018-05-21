package com.mapswithme.maps.settings;

import android.content.Context;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.XmlRes;
import android.support.v4.content.ContextCompat;
import android.support.v7.preference.PreferenceFragmentCompat;
import android.view.View;

import com.mapswithme.maps.R;
import com.mapswithme.util.Config;
import com.mapswithme.util.CoreInitChecker;
import com.mapswithme.util.DefaultCoreChecker;
import com.mapswithme.util.ThemeUtils;
import com.mapswithme.util.UiUtils;
import com.mapswithme.util.Utils;

abstract class BaseXmlSettingsFragment extends PreferenceFragmentCompat
{
  protected abstract @XmlRes int getXmlResources();

  @Override
  public void onCreatePreferences(Bundle bundle, String root)
  {
    setPreferencesFromResource(getXmlResources(), root);
  }

  @Override
  public final void onAttach(Context context)
  {
    super.onAttach(context);
    if (getChecker().check(context))
    {
      onSafeAttach(context);
      return;
    }
    Utils.detachFragment(context, this);
  }

  @NonNull
  protected CoreInitChecker getChecker()
  {
    return new DefaultCoreChecker();
  }

  protected void onSafeAttach(@NonNull Context context)
  {
    // No default implementation.
  }

  @Override
  public final void onCreate(Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);
  }

  @Override
  public void onResume()
  {
    super.onResume();
    org.alohalytics.Statistics.logEvent("$onResume", getClass().getSimpleName() + ":" +
                                                     UiUtils.deviceOrientationAsString(getActivity()));
  }

  @Override
  public void onPause()
  {
    super.onPause();
    org.alohalytics.Statistics.logEvent("$onPause", getClass().getSimpleName() + ":" +
                                                    UiUtils.deviceOrientationAsString(getActivity()));
  }

  @Override
  public void onViewCreated(View view, @Nullable Bundle savedInstanceState)
  {
    super.onViewCreated(view, savedInstanceState);

    String theme = Config.getCurrentUiTheme();
    int color;
    if (ThemeUtils.isDefaultTheme(theme))
      color = ContextCompat.getColor(getContext(), R.color.bg_cards);
    else
      color = ContextCompat.getColor(getContext(), R.color.bg_cards_night);
    view.setBackgroundColor(color);
  }

  protected SettingsActivity getSettingsActivity()
  {
    return (SettingsActivity) getActivity();
  }
}
