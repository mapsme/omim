package com.mapswithme.maps.settings;

import android.app.Activity;
import android.app.ProgressDialog;
import android.content.DialogInterface;
import android.os.Bundle;
import android.support.v7.app.AlertDialog;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ListView;
import android.widget.TextView;

import com.mapswithme.maps.R;
import com.mapswithme.maps.base.OnBackPressListener;
import com.mapswithme.maps.widget.BaseShadowController;
import com.mapswithme.util.Constants;
import com.mapswithme.util.Utils;

import java.util.List;

public class StoragePathFragment extends BaseSettingsFragment
                              implements StoragePathManager.MoveFilesListener,
                                         OnBackPressListener,
                                         AdapterView.OnItemClickListener
{
  private TextView mHeader;
  private ListView mList;
  private ProgressDialog mProgress;

  private StoragePathAdapter mAdapter;
  private StoragePathManager mPathManager = new StoragePathManager();

  @Override
  protected int getLayoutRes()
  {
    return R.layout.fragment_prefs_storage;
  }

  @Override
  protected BaseShadowController createShadowController()
  {
    return null;
  }

  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
  {
    super.onCreateView(inflater, container, savedInstanceState);

    mHeader = (TextView) mFrame.findViewById(R.id.header);
    mList = (ListView) mFrame.findViewById(R.id.list);
    mList.setOnItemClickListener(this);

    return mFrame;
  }

  @Override
  public void onResume()
  {
    super.onResume();
    mPathManager.startExternalStorageWatching(getActivity(), new StoragePathManager.OnStorageListChangedListener()
    {
      @Override
      public void onStorageListChanged(List<String> storageItems)
      {
        updateList();
      }
    }, this);

    if (mAdapter == null)
      mAdapter = new StoragePathAdapter(getActivity());

    updateList();
    mList.setAdapter(mAdapter);
  }

  @Override
  public void onPause()
  {
    super.onPause();
    mPathManager.stopExternalStorageWatching();
  }

  private void updateList()
  {
    long dirSize = StoragePathManager.getMwmDirSize();
    mHeader.setText(getString(R.string.maps) + ": " + getSizeString(dirSize));

    if (mAdapter != null)
      mAdapter.update(mPathManager.getStorages());
  }

  @Override
  public void moveFilesFinished(String newPath)
  {
    if (!isAdded())
      return;

    showProgress(false);
    updateList();
  }

  @Override
  public void moveFilesFailed(String errorCode)
  {
    if (!isAdded())
      return;

    final Activity activity = getActivity();
    if (activity.isFinishing())
      return;

    showProgress(false);
    final String message = "Failed to move maps with internal error: " + errorCode;
    new AlertDialog.Builder(activity)
        .setTitle(message)
        .setPositiveButton(R.string.report_a_bug, new DialogInterface.OnClickListener()
        {
          @Override
          public void onClick(DialogInterface dialog, int which)
          {
            Utils.sendSupportMail(activity, message);
          }
        }).show();
  }

  static String getSizeString(long size)
  {
    final String units[] = {"Kb", "Mb", "Gb"};

    long current = Constants.KB;
    int i = 0;
    for (; i < units.length; ++i)
    {
      final long bound = Constants.KB * current;
      if (size < bound)
        break;

      current = bound;
    }

    // left 1 digit after the comma and add postfix string
    return String.format("%.1f %s", (double) size / current, units[i]);
  }

  @Override
  public boolean onBackPressed()
  {
    SettingsActivity activity = (SettingsActivity) getActivity();
    if (activity.onIsMultiPane())
    {
      activity.switchToHeader(R.id.group_map);
      return true;
    }

    return false;
  }

  @Override
  public void onItemClick(AdapterView<?> parent, View view, final int position, long id)
  {
    if (!mAdapter.isStorageBigEnough(position) || position == mAdapter.getStorageIndex())
      return;

    new AlertDialog.Builder(getActivity())
        .setCancelable(false)
        .setTitle(R.string.move_maps)
        .setPositiveButton(R.string.ok, new DialogInterface.OnClickListener()
        {
          @Override
          public void onClick(DialogInterface dlg, int which)
          {
            mPathManager.changeStorage(mAdapter.getItem(position).getMwmRootPath());
            showProgress(true);
          }
        })
        .setNegativeButton(R.string.cancel, null)
        .show();
  }

  private void showProgress(boolean show)
  {
    if (show)
    {
      mProgress = new ProgressDialog(getActivity());
      mProgress.setMessage(getString(R.string.wait_several_minutes));
      mProgress.setProgressStyle(ProgressDialog.STYLE_SPINNER);
      mProgress.setIndeterminate(true);
      mProgress.setCancelable(false);
      mProgress.show();
    }
    else
      mProgress.dismiss();
  }
}
