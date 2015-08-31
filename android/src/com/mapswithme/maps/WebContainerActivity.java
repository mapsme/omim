package com.mapswithme.maps;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.StringRes;
import android.support.v4.app.Fragment;
import com.mapswithme.maps.base.BaseToolbarActivity;

public class WebContainerActivity extends BaseToolbarActivity
{
  private static final String EXTRA_TITLE = "title";
  static final String EXTRA_URL = "url";

  private int mTitleRes;
  private String mUrl;


  String getUrl()
  {
    return mUrl;
  }

  @Override
  protected int getToolbarTitle()
  {
    return mTitleRes;
  }

  @Override
  protected Class<? extends Fragment> getFragmentClass()
  {
    return WebContainerFragment.class;
  }

  @Override
  protected void onCreate(Bundle state)
  {
    mUrl = getIntent().getStringExtra(EXTRA_URL);
    mTitleRes = getIntent().getIntExtra(EXTRA_TITLE, 0);

    super.onCreate(state);
  }

  public static void show(Activity activity, String url, @StringRes int titleRes)
  {
    activity.startActivity(new Intent(activity, WebContainerActivity.class)
                               .putExtra(EXTRA_URL, url)
                               .putExtra(EXTRA_TITLE, titleRes));
  }
}
