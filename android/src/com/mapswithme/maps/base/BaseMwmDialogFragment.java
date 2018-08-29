package com.mapswithme.maps.base;

import android.app.Application;
import android.content.Context;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.StyleRes;
import android.support.v4.app.DialogFragment;

import com.mapswithme.maps.R;
import com.mapswithme.util.ThemeUtils;

public class BaseMwmDialogFragment extends DialogFragment
{
  @StyleRes
  protected final int getFullscreenTheme()
  {
    return ThemeUtils.isNightTheme() ? getFullscreenDarkTheme() : getFullscreenLightTheme();
  }

  protected int getStyle()
  {
    return STYLE_NORMAL;
  }

  protected @StyleRes int getCustomTheme()
  {
    return 0;
  }

  @Override
  public void onCreate(@Nullable Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);

    int style = getStyle();
    int theme = getCustomTheme();
    if (style != STYLE_NORMAL || theme != 0)
      //noinspection WrongConstant
      setStyle(style, theme);
  }

  @Override
  public void onResume()
  {
    super.onResume();
    org.alohalytics.Statistics.logEvent("$onResume", getClass().getSimpleName()
        + ":" + com.mapswithme.util.UiUtils.deviceOrientationAsString(getActivity()));
  }

  @Override
  public void onPause()
  {
    super.onPause();
    org.alohalytics.Statistics.logEvent("$onPause", getClass().getSimpleName());
  }

  @StyleRes
  protected int getFullscreenLightTheme()
  {
    return R.style.MwmTheme_DialogFragment_Fullscreen;
  }

  @StyleRes
  protected int getFullscreenDarkTheme()
  {
    return R.style.MwmTheme_DialogFragment_Fullscreen_Night;
  }

  @NonNull
  protected Application getAppContextOrThrow()
  {
    Context context = getContext();
    if (context == null)
      throw new IllegalStateException("Before call this method make sure that the context exist");
    return (Application) context.getApplicationContext();
  }
}
