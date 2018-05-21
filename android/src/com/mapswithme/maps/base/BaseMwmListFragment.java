package com.mapswithme.maps.base;

import android.content.Context;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.v4.app.ListFragment;
import android.support.v7.widget.Toolbar;
import android.view.View;

import com.mapswithme.maps.R;
import com.mapswithme.util.CoreInitChecker;
import com.mapswithme.util.DefaultCoreChecker;
import com.mapswithme.util.UiUtils;
import com.mapswithme.util.Utils;

@Deprecated
public abstract class BaseMwmListFragment extends ListFragment
{
  private Toolbar mToolbar;

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
  public void onViewCreated(View view, Bundle savedInstanceState)
  {
    super.onViewCreated(view, savedInstanceState);

    mToolbar = (Toolbar) view.findViewById(R.id.toolbar);
    if (mToolbar != null)
    {
      UiUtils.showHomeUpButton(mToolbar);
      mToolbar.setNavigationOnClickListener(new View.OnClickListener()
      {
        @Override
        public void onClick(View v)
        {
          Utils.navigateToParent(getActivity());
        }
      });
    }
  }

  public Toolbar getToolbar()
  {
    return mToolbar;
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
    org.alohalytics.Statistics.logEvent("$onPause", getClass().getSimpleName());
  }
}
