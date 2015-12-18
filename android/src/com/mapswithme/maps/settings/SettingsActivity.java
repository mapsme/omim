package com.mapswithme.maps.settings;

import android.app.Activity;
import android.app.Fragment;
import android.content.res.Configuration;
import android.media.AudioManager;
import android.os.Bundle;
import android.preference.PreferenceActivity;
import android.support.annotation.NonNull;
import android.support.annotation.StringRes;
import android.support.v7.app.AppCompatDelegate;
import android.support.v7.widget.Toolbar;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

import com.mapswithme.maps.MwmApplication;
import com.mapswithme.maps.R;
import com.mapswithme.maps.base.BaseActivity;
import com.mapswithme.maps.base.BaseDelegate;
import com.mapswithme.maps.base.OnBackPressListener;
import com.mapswithme.util.FragmentListHelper;
import com.mapswithme.util.ThemeUtils;
import com.mapswithme.util.UiUtils;
import com.mapswithme.util.statistics.AlohaHelper;
import com.mapswithme.util.statistics.Statistics;

public class SettingsActivity extends PreferenceActivity
                           implements BaseActivity
{
  private final BaseDelegate mBaseDelegate = new BaseDelegate(this);
  private final FragmentListHelper mFragmentListHelper = new FragmentListHelper();
  private AppCompatDelegate mDelegate;
  private CharSequence mNextBreadcrumb;
  private final Map<Long, Header> mHeaders = new HashMap<>();

  @Override
  public Activity get()
  {
    return this;
  }

  @Override
  public int getThemeResourceId(String theme)
  {
    if (ThemeUtils.THEME_DEFAULT.equals(theme))
      return R.style.MwmTheme_Settings;

    if (ThemeUtils.THEME_NIGHT.equals(theme))
      return R.style.MwmTheme_Night_Settings;

    throw new IllegalArgumentException("Attempt to apply unsupported theme: " + theme);
  }

  private AppCompatDelegate getDelegate()
  {
    if (mDelegate == null)
      mDelegate = AppCompatDelegate.create(this, null);

    return mDelegate;
  }

  @Override
  public void onAttachFragment(Fragment fragment)
  {
    mFragmentListHelper.onAttachFragment(fragment);
  }

  @Override
  public void onBuildHeaders(List<Header> target)
  {
    loadHeadersFromResource(R.xml.prefs_headers, target);

    mHeaders.clear();
    for (Header h : target)
      mHeaders.put(h.id, h);
  }

  @Override
  public void onHeaderClick(@NonNull Header header, int position)
  {
    if (header.id == R.id.group_map)
      Statistics.INSTANCE.trackEvent(Statistics.EventName.Settings.GROUP_MAP);
    else if (header.id == R.id.group_route)
      Statistics.INSTANCE.trackEvent(Statistics.EventName.Settings.GROUP_ROUTE);
    else if (header.id == R.id.group_misc)
      Statistics.INSTANCE.trackEvent(Statistics.EventName.Settings.GROUP_MISC);
    else if (header.id == R.id.help)
    {
      Statistics.INSTANCE.trackEvent(Statistics.EventName.Settings.HELP);
      AlohaHelper.logClick(AlohaHelper.Settings.HELP);
    }
    else if (header.id == R.id.about)
    {
      Statistics.INSTANCE.trackEvent(Statistics.EventName.Settings.ABOUT);
      AlohaHelper.logClick(AlohaHelper.Settings.ABOUT);
    }

    super.onHeaderClick(header, position);
  }

  @Override
  protected void onCreate(Bundle savedInstanceState)
  {
    mBaseDelegate.onCreate();
    getDelegate().installViewFactory();
    getDelegate().onCreate(savedInstanceState);

    super.onCreate(savedInstanceState);
    setVolumeControlStream(AudioManager.STREAM_MUSIC);

    // Hack to attach Toolbar and make it work on native PreferenceActivity
    ViewGroup root = (ViewGroup)findViewById(android.R.id.list).getParent().getParent().getParent();
    View toolbarHolder = LayoutInflater.from(this).inflate(R.layout.toolbar_default, root, false);
    Toolbar toolbar = (Toolbar) toolbarHolder.findViewById(R.id.toolbar);
    UiUtils.showHomeUpButton(toolbar);

    // First, add toolbar view to UI.
    root.addView(toolbarHolder, 0);
    // Second, attach it as ActionBar (it does not add view, so we need previous step).
    getDelegate().setSupportActionBar(toolbar);

    MwmApplication.get().initNativeCore();
    MwmApplication.get().initCounters();
  }

  @Override
  protected void onPostCreate(Bundle savedInstanceState)
  {
    super.onPostCreate(savedInstanceState);
    mBaseDelegate.onPostCreate();
    getDelegate().onPostCreate(savedInstanceState);
  }

  @Override
  public void setContentView(int layoutResID)
  {
    getDelegate().setContentView(layoutResID);
  }

  @Override
  public void setContentView(View view)
  {
    getDelegate().setContentView(view);
  }

  @Override
  public void setContentView(View view, ViewGroup.LayoutParams params)
  {
    getDelegate().setContentView(view, params);
  }

  @Override
  public void addContentView(View view, ViewGroup.LayoutParams params)
  {
    getDelegate().addContentView(view, params);
  }

  @Override
  protected void onTitleChanged(CharSequence title, int color)
  {
    super.onTitleChanged(title, color);
    getDelegate().setTitle(title);
  }

  @Override
  public void invalidateOptionsMenu()
  {
    super.invalidateOptionsMenu();
    getDelegate().invalidateOptionsMenu();
  }

  @Override
  public void onConfigurationChanged(Configuration newConfig)
  {
    super.onConfigurationChanged(newConfig);
    getDelegate().onConfigurationChanged(newConfig);
  }

  @Override
  protected void onDestroy()
  {
    super.onDestroy();
    mBaseDelegate.onDestroy();
    getDelegate().onDestroy();
  }

  @Override
  protected boolean isValidFragment(String fragmentName)
  {
    return true;
  }

  @Override
  protected void onStart()
  {
    super.onStart();
    mBaseDelegate.onStart();
  }

  @Override
  protected void onStop()
  {
    super.onStop();
    mBaseDelegate.onStop();
    getDelegate().onStop();
  }

  @Override
  protected void onResume()
  {
    super.onResume();
    mBaseDelegate.onResume();
  }

  @Override
  protected void onPostResume()
  {
    super.onPostResume();
    mBaseDelegate.onPostResume();
    getDelegate().onPostResume();
  }

  @Override
  protected void onPause()
  {
    super.onPause();
    mBaseDelegate.onPause();
  }

  @Override
  public boolean onOptionsItemSelected(MenuItem item)
  {
    if (item.getItemId() == android.R.id.home)
    {
      super.onBackPressed();
      return true;
    }

    return super.onOptionsItemSelected(item);
  }

  @Override
  public void onBackPressed()
  {
    for (Fragment f : mFragmentListHelper.getFragments())
      if ((f instanceof OnBackPressListener) && ((OnBackPressListener)f).onBackPressed())
        return;

    super.onBackPressed();
  }

  @Override
  public void showBreadCrumbs(CharSequence title, CharSequence shortTitle)
  {
    if (mNextBreadcrumb != null)
    {
      title = mNextBreadcrumb;
      mNextBreadcrumb = null;
    }

    super.showBreadCrumbs(title, shortTitle);
  }

  public void switchToHeader(long id)
  {
    Header h = mHeaders.get(id);
    if (h != null)
      switchToHeader(h);
  }

  public void switchToFragment(Class<? extends BaseSettingsFragment> fragmentClass, @StringRes int breadcrumb)
  {
    mNextBreadcrumb = getString(breadcrumb);
    switchToHeader(fragmentClass.getName(), null);
  }
}
