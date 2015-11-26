package com.mapswithme.maps.widget;

import android.app.Dialog;
import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.v4.app.DialogFragment;
import android.support.v4.app.Fragment;
import android.support.v7.app.AlertDialog;
import android.view.LayoutInflater;
import android.view.View;

import com.mapswithme.maps.R;
import com.mapswithme.maps.base.BaseMwmDialogFragment;
import com.mapswithme.maps.base.BaseMwmFragmentActivity;
import com.mapswithme.maps.settings.StoragePathManager;
import com.mapswithme.maps.settings.StorageUtils;
import com.mapswithme.util.UiUtils;

public class SdcardUnavailableDialogFragment extends BaseMwmDialogFragment
{
  public static void checkSdcard(BaseMwmFragmentActivity activity)
  {
    if (StoragePathManager.isMwmPathWritable())
      return;

    final DialogFragment fragment = (DialogFragment) Fragment.instantiate(activity, SdcardUnavailableDialogFragment.class.getName());
    fragment.show(activity.getSupportFragmentManager(), null);
  }

  @NonNull
  @Override
  public Dialog onCreateDialog(Bundle savedInstanceState)
  {
    return new AlertDialog.Builder(getActivity())
           .setView(buildView())
           .setCancelable(false)
           .create();
  }

  private View buildView()
  {
    final View view = LayoutInflater.from(getActivity()).inflate(R.layout.dialog_sdcard_unavailable, null);

    final long primaryFreeSize = StorageUtils.getFreeBytesAtPath(StoragePathManager.getSettingsPath());
    final long mapsSize = StoragePathManager.getMwmDirSize();
    final View button = view.findViewById(R.id.btn__use_internal_storage);
    if (primaryFreeSize < mapsSize)
      UiUtils.hide(button);
    else
      button.setOnClickListener(new View.OnClickListener()
      {
        @Override
        public void onClick(View v)
        {
          StoragePathManager.resetStorage(StoragePathManager.getSettingsPath());
          dismiss();
          Intent intent = getActivity().getIntent();
          intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
          getActivity().finish();
          startActivity(intent);
        }
      });
    view.findViewById(R.id.btn__try_again).setOnClickListener(new View.OnClickListener()
    {
      @Override
      public void onClick(View v)
      {
        dismiss();
        checkSdcard((BaseMwmFragmentActivity) getActivity());
      }
    });
    view.findViewById(R.id.btn__cancel).setOnClickListener(new View.OnClickListener()
    {
      @Override
      public void onClick(View v)
      {
        dismiss();
      }
    });

    return view;
  }
}
