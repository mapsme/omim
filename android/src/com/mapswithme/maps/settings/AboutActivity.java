package com.mapswithme.maps.settings;

import android.support.v4.app.Fragment;
import com.mapswithme.maps.base.BaseToolbarActivity;

public class AboutActivity extends BaseToolbarActivity
{
  @Override
  protected Class<? extends Fragment> getFragmentClass()
  {
    return AboutFragment.class;
  }
}
