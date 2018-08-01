package com.mapswithme.maps.bookmarks;

import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;

import com.mapswithme.maps.R;
import com.mapswithme.maps.SplashActivity;
import com.mapswithme.maps.base.BaseToolbarActivity;
import com.mapswithme.maps.base.OnBackPressListener;
import com.mapswithme.util.Utils;

public class BookmarksCatalogActivity extends BaseToolbarActivity
{
  public static final int REQ_CODE_CATALOG = 101;
  public static final String EXTRA_DOWNLOADED_CATEGORY = "extra_downloaded_category";

  @Override
  protected void safeOnCreate(@Nullable Bundle savedInstanceState)
  {
    // TODO: Move this logic to BaseMwmFragmentActivity after release 8.3.x
    // to avoid code duplication for other activities that will be in the same user case.
    // https://jira.mail.ru/browse/MAPSME-8195
    Intent intent = getIntent().getParcelableExtra(SplashActivity.EXTRA_INTENT);
    if (intent != null)
      setIntent(intent);
    super.safeOnCreate(savedInstanceState);
    getToolbar().setNavigationIcon(R.drawable.ic_clear);
  }

  @Override
  protected Class<? extends Fragment> getFragmentClass()
  {
    return BookmarksCatalogFragment.class;
  }

  @Override
  protected boolean onBackPressedInternal(@NonNull Fragment currentFragment)
  {
    return Utils.<OnBackPressListener>castTo(currentFragment).onBackPressed();
  }

  @Override
  protected void onHomeOptionItemSelected()
  {
    finish();
  }
}
