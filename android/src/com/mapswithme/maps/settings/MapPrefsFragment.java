package com.mapswithme.maps.settings;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.TwoStatePreference;
import android.support.v7.app.AlertDialog;

import java.util.List;

import com.mapswithme.maps.downloader.country.OldActiveCountryTree;
import com.mapswithme.maps.Framework;
import com.mapswithme.maps.R;
import com.mapswithme.maps.location.TrackRecorder;
import com.mapswithme.util.Config;
import com.mapswithme.util.ThemeSwitcher;
import com.mapswithme.util.Yota;
import com.mapswithme.util.concurrency.UiThread;
import com.mapswithme.util.statistics.AlohaHelper;
import com.mapswithme.util.statistics.Statistics;

public class MapPrefsFragment extends BaseXmlSettingsFragment
{
  private final StoragePathManager mPathManager = new StoragePathManager();
  private Preference mStoragePref;

  private boolean singleStorageOnly()
  {
    return Yota.isFirstYota() || !mPathManager.hasMoreThanOneStorage();
  }

  private void updateStoragePrefs()
  {
    Preference old = findPreference(getString(R.string.pref_storage));

    if (singleStorageOnly())
    {
      if (old != null)
        getPreferenceScreen().removePreference(old);
    }
    else
    {
      if (old == null)
        getPreferenceScreen().addPreference(mStoragePref);
    }
  }

  @Override
  protected int getXmlResources()
  {
    return R.xml.prefs_map;
  }

  @Override
  public void onCreate(Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);

    mStoragePref = findPreference(getString(R.string.pref_storage));
    updateStoragePrefs();

    mStoragePref.setOnPreferenceClickListener(new Preference.OnPreferenceClickListener()
    {
      @Override
      public boolean onPreferenceClick(Preference preference)
      {
        if (OldActiveCountryTree.isDownloadingActive())
          new AlertDialog.Builder(getActivity())
              .setTitle(getString(R.string.downloading_is_active))
              .setMessage(getString(R.string.cant_change_this_setting))
              .setPositiveButton(getString(R.string.ok), null)
              .show();
        else
          ((SettingsActivity)getActivity()).switchToFragment(StoragePathFragment.class, R.string.maps_storage);

        return true;
      }
    });

    Preference pref = findPreference(getString(R.string.pref_munits));
    ((ListPreference)pref).setValue(String.valueOf(UnitLocale.getUnits()));
    pref.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener()
    {
      @Override
      public boolean onPreferenceChange(Preference preference, Object newValue)
      {
        UnitLocale.setUnits(Integer.parseInt((String) newValue));
        Statistics.INSTANCE.trackEvent(Statistics.EventName.Settings.UNITS);
        AlohaHelper.logClick(AlohaHelper.Settings.CHANGE_UNITS);
        return true;
      }
    });

    pref = findPreference(getString(R.string.pref_show_zoom_buttons));
    ((TwoStatePreference)pref).setChecked(Config.showZoomButtons());
    pref.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener()
    {
      @Override
      public boolean onPreferenceChange(Preference preference, Object newValue)
      {
        Statistics.INSTANCE.trackEvent(Statistics.EventName.Settings.ZOOM);
        Config.setShowZoomButtons((Boolean) newValue);
        return true;
      }
    });

    String curTheme = Config.getUiThemeSettings();
    final ListPreference stylePref = (ListPreference)findPreference(getString(R.string.pref_map_style));
    stylePref.setValue(curTheme);
    stylePref.setSummary(stylePref.getEntry());
    stylePref.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener()
    {
      @Override
      public boolean onPreferenceChange(Preference preference, Object newValue)
      {
        String themeName = (String)newValue;
        if (!Config.setUiThemeSettings(themeName))
          return true;

        ThemeSwitcher.restart();
        Statistics.INSTANCE.trackEvent(Statistics.EventName.Settings.MAP_STYLE,
                                       Statistics.params().add(Statistics.EventParam.NAME, themeName));

        UiThread.runLater(new Runnable()
        {
          @Override
          public void run()
          {
            stylePref.setSummary(stylePref.getEntry());
          }
        });

        return true;
      }
    });

    TwoStatePreference prefAutodownload = (TwoStatePreference)findPreference(getString(R.string.pref_autodownload));
    prefAutodownload.setChecked(Config.isAutoDownloadEnabled());
    prefAutodownload.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener()
    {
      @Override
      public boolean onPreferenceChange(Preference preference, Object newValue)
      {
        Config.setAutoDownloadEnabled((Boolean)newValue);
        return true;
      }
    });

    final Framework.Params3dMode _3d = new Framework.Params3dMode();
    Framework.nativeGet3dMode(_3d);

    final TwoStatePreference pref3dBuildings = (TwoStatePreference)findPreference(getString(R.string.pref_3d_buildings));
    pref3dBuildings.setChecked(_3d.buildings);

    pref3dBuildings.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener()
    {
      @Override
      public boolean onPreferenceChange(Preference preference, Object newValue)
      {
        Framework.nativeSet3dMode(_3d.enabled, (Boolean)newValue);
        return true;
      }
    });

    pref = findPreference(getString(R.string.pref_yota));
    if (Yota.isFirstYota())
      pref.setOnPreferenceClickListener(new Preference.OnPreferenceClickListener()
      {
        @Override
        public boolean onPreferenceClick(Preference preference)
        {
          startActivity(new Intent(Yota.ACTION_PREFERENCE));
          return false;
        }
      });
    else
      getPreferenceScreen().removePreference(pref);

    final ListPreference trackPref = (ListPreference)findPreference(getString(R.string.pref_track_record));
    String value = (TrackRecorder.isEnabled() ? String.valueOf(TrackRecorder.getDuration()) : "0");
    trackPref.setValue(value);
    trackPref.setSummary(trackPref.getEntry());
    trackPref.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener()
    {
      @Override
      public boolean onPreferenceChange(final Preference preference, Object newValue)
      {
        int value = Integer.valueOf((String)newValue);
        TrackRecorder.setEnabled(value != 0);

        if (value != 0)
          TrackRecorder.setDuration(value);

        UiThread.runLater(new Runnable()
        {
          @Override
          public void run()
          {
            trackPref.setSummary(trackPref.getEntry());
          }
        });
        return true;
      }
    });
  }

  @Override
  public void onAttach(Activity activity)
  {
    super.onAttach(activity);
    mPathManager.startExternalStorageWatching(activity, new StoragePathManager.OnStorageListChangedListener()
    {
      @Override
      public void onStorageListChanged(List<StorageItem> storageItems, int currentStorageIndex)
      {
        updateStoragePrefs();
      }
    }, null);
  }

  @Override
  public void onDetach()
  {
    super.onDetach();
    mPathManager.stopExternalStorageWatching();
  }
}
