package com.mapswithme.maps.ugc.routes;

import androidx.fragment.app.Fragment;

import com.mapswithme.maps.base.BaseToolbarActivity;

public class UgcRouteTagsActivity extends BaseToolbarActivity
{
  public static final String EXTRA_TAGS = "selected_tags";

  @Override
  protected Class<? extends Fragment> getFragmentClass()
  {
    return UgcRouteTagsFragment.class;
  }
}
