package com.mapswithme.maps.gdpr;

import android.support.v4.app.Fragment;

import com.mapswithme.maps.base.BaseToolbarActivity;

public class MwmOptOutActivity extends BaseToolbarActivity
{
  @Override
  protected Class<? extends Fragment> getFragmentClass()
  {
    return OptOutFragment.class;
  }
}
