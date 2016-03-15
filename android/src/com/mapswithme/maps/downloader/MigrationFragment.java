package com.mapswithme.maps.downloader;

import android.location.Location;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.annotation.StringRes;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.TextView;

import com.mapswithme.maps.MwmActivity;
import com.mapswithme.maps.R;
import com.mapswithme.maps.base.BaseMwmFragment;
import com.mapswithme.maps.base.OnBackPressListener;
import com.mapswithme.maps.location.LocationHelper;
import com.mapswithme.maps.widget.WheelProgressView;
import com.mapswithme.util.UiUtils;
import com.mapswithme.util.statistics.Statistics;

public class MigrationFragment extends BaseMwmFragment
                            implements OnBackPressListener,
                                       MigrationController.Container
{
  private TextView mError;
  private View mPrepare;
  private WheelProgressView mProgress;
  private Button mButtonPrimary;
  private Button mButtonSecondary;

  private final View.OnClickListener mButtonClickListener = new View.OnClickListener()
  {
    @Override
    public void onClick(View v)
    {
      boolean keepOld = (v == mButtonPrimary);

      Statistics.INSTANCE.trackEvent(Statistics.EventName.DOWNLOADER_MIGRATION_STARTED,
                                     Statistics.params().add(Statistics.EventParam.TYPE, keepOld ? "all_maps"
                                                                                                 : "current_map"));
      MigrationController.get().start(keepOld);
    }
  };

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
  {
    return inflater.inflate(R.layout.fragment_migrate, container, false);
  }

  @Override
  public void onViewCreated(View view, @Nullable Bundle savedInstanceState)
  {
    super.onViewCreated(view, savedInstanceState);

    mError = (TextView) view.findViewById(R.id.error);
    mPrepare = view.findViewById(R.id.preparation);
    mProgress = (WheelProgressView) view.findViewById(R.id.progress);
    mButtonPrimary = (Button) view.findViewById(R.id.button_primary);
    mButtonSecondary = (Button) view.findViewById(R.id.button_secondary);

    mButtonPrimary.setOnClickListener(mButtonClickListener);
    mButtonSecondary.setOnClickListener(mButtonClickListener);

    mProgress.setOnClickListener(new View.OnClickListener()
    {
      @Override
      public void onClick(View v)
      {
        MigrationController.get().cancel();
      }
    });

    MigrationController.get().attach(this);

    Statistics.INSTANCE.trackEvent(Statistics.EventName.DOWNLOADER_MIGRATION_DIALOG_SEEN);
  }

  @Override
  public void onDestroyView()
  {
    super.onDestroyView();
    MigrationController.get().detach();
  }

  @Override
  public void onResume()
  {
    super.onResume();
    MigrationController.get().restore();
  }

  @Override
  public void setReadyState()
  {
    UiUtils.show(mButtonPrimary, mButtonSecondary);
    UiUtils.hide(mPrepare, mProgress, mError);

    Location loc = LocationHelper.INSTANCE.getLastLocation();
    if (loc == null)
      UiUtils.setTextAndShow(mError, getString(R.string.undefined_location));

    mButtonPrimary.setEnabled(loc != null);
    UiUtils.updateAccentButton(mButtonPrimary);
  }

  @Override
  public void setProgressState()
  {
    UiUtils.show(mPrepare, mProgress);
    UiUtils.hide(mError, mButtonPrimary, mButtonSecondary);
  }

  @Override
  public void setErrorState(int code)
  {
    setReadyState();

    UiUtils.show(mError);

    @StringRes int text;
    switch (code)
    {
    case CountryItem.ERROR_OOM:
      text = R.string.downloader_no_space_title;
      break;

    case CountryItem.ERROR_NO_INTERNET:
      text = R.string.common_check_internet_connection_dialog;
      break;

    default:
      text = R.string.country_status_download_failed;
    }

    mError.setText(text);
  }

  @Override
  public void onComplete()
  {
    if (!isAdded())
      return;

    if (getActivity() instanceof MwmActivity)
      ((MwmActivity)getActivity()).showDownloader(false);
    else
      getActivity().recreate();

    Statistics.INSTANCE.trackEvent(Statistics.EventName.DOWNLOADER_MIGRATION_COMPLETE);
  }

  @Override
  public void setProgress(int percents)
  {
    mProgress.setProgress(percents);
  }

  @Override
  public boolean onBackPressed()
  {
    return false;
  }
}
