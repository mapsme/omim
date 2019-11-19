package com.mapswithme.maps.settings;

import android.content.Context;
import android.os.Bundle;
import androidx.annotation.Nullable;
import androidx.annotation.XmlRes;
import androidx.core.content.ContextCompat;
import androidx.preference.PreferenceFragmentCompat;
import android.view.View;

import com.mapswithme.maps.R;
import com.mapswithme.util.Config;
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
  public void onAttach(Context context)
  {
    super.onAttach(context);
    Utils.detachFragmentIfCoreNotInitialized(context, this);
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
