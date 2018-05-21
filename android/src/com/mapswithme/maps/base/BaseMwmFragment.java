package com.mapswithme.maps.base;

import android.content.Context;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;

import com.mapswithme.util.CoreInitChecker;
import com.mapswithme.util.DefaultCoreChecker;
import com.mapswithme.util.Utils;

public class BaseMwmFragment extends Fragment
{
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
  public final void onCreate(@Nullable Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);
  }

  @Override
  public void onResume()
  {
    super.onResume();
    org.alohalytics.Statistics.logEvent("$onResume", this.getClass().getSimpleName()
        + ":" + com.mapswithme.util.UiUtils.deviceOrientationAsString(getActivity()));
  }

  @Override
  public void onPause()
  {
    super.onPause();
    org.alohalytics.Statistics.logEvent("$onPause", this.getClass().getSimpleName());
  }

  public BaseMwmFragmentActivity getMwmActivity()
  {
    return (BaseMwmFragmentActivity) getActivity();
  }
}
