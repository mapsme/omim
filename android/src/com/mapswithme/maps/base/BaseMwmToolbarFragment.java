package com.mapswithme.maps.base;

import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;
import android.view.View;

import com.mapswithme.maps.widget.ToolbarController;

public class BaseMwmToolbarFragment extends Fragment
{
  protected ToolbarController mToolbarController;

  @Override
  public void onViewCreated(View view, @Nullable Bundle savedInstanceState)
  {
    super.onViewCreated(view, savedInstanceState);
    mToolbarController = onCreateToolbarController(view);
  }

  protected ToolbarController onCreateToolbarController(@NonNull View root)
  {
    return new ToolbarController(root, getActivity());
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
}
