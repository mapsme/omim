package com.mapswithme.maps.downloader;

import android.app.Dialog;
import android.content.DialogInterface;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.StringRes;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.FragmentTransaction;
import android.view.View;
import android.view.Window;
import android.widget.TextView;

import com.mapswithme.maps.Framework;
import com.mapswithme.maps.R;
import com.mapswithme.maps.base.BaseMwmDialogFragment;
import com.mapswithme.maps.news.BaseNewsFragment;
import com.mapswithme.maps.widget.WheelProgressView;
import com.mapswithme.util.Constants;
import com.mapswithme.util.StringUtils;
import com.mapswithme.util.UiUtils;
import com.mapswithme.util.Utils;
import com.mapswithme.util.log.Logger;
import com.mapswithme.util.log.LoggerFactory;
import com.mapswithme.util.statistics.Statistics;

import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import static com.mapswithme.util.statistics.Statistics.EventName.DOWNLOADER_DIALOG_CANCEL;
import static com.mapswithme.util.statistics.Statistics.EventName.DOWNLOADER_DIALOG_DOWNLOAD;
import static com.mapswithme.util.statistics.Statistics.EventName.DOWNLOADER_DIALOG_HIDE;
import static com.mapswithme.util.statistics.Statistics.EventName.DOWNLOADER_DIALOG_LATER;
import static com.mapswithme.util.statistics.Statistics.EventName.DOWNLOADER_DIALOG_MANUAL_DOWNLOAD;
import static com.mapswithme.util.statistics.Statistics.EventName.DOWNLOADER_DIALOG_SHOW;

public class UpdaterDialogFragment extends BaseMwmDialogFragment
{
  private static final Logger LOGGER = LoggerFactory.INSTANCE.getLogger(LoggerFactory.Type.DOWNLOADER);
  private static final String TAG = UpdaterDialogFragment.class.getSimpleName();

  private static final String EXTRA_LEFTOVER_MAPS = "extra_leftover_maps";
  private static final String EXTRA_PROCESSED_MAP_ID = "extra_processed_map_id";
  private static final String EXTRA_COMMON_STATUS_RES_ID = "extra_common_status_res_id";

  private static final String ARG_UPDATE_IMMEDIATELY = "arg_update_immediately";
  private static final String ARG_TOTAL_SIZE = "arg_total_size";
  private static final String ARG_TOTAL_SIZE_BYTES = "arg_total_size_bytes";
  private static final String ARG_OUTDATED_MAPS = "arg_outdated_maps";

  private TextView mTitle;
  private TextView mUpdateBtn;
  private WheelProgressView mProgressBar;
  private TextView mFinishBtn;
  private View mInfo;
  private TextView mRelativeStatus;

  @Nullable
  private String mTotalSize;
  private long mTotalSizeBytes;
  private boolean mAutoUpdate;
  @Nullable
  private String[] mOutdatedMaps;
  /**
   * Stores maps which are left to finish autoupdating process.
   */
  @Nullable
  private HashSet<String> mLeftoverMaps;

  @Nullable
  private BaseNewsFragment.NewsDialogListener mDoneListener;

  @Nullable
  private DetachableStorageCallback mStorageCallback;

  @Nullable
  private String mProcessedMapId;
  @StringRes
  private int mCommonStatusResId = Utils.INVALID_ID;

  private void finish()
  {
    dismiss();
    if (mDoneListener != null)
      mDoneListener.onDialogDone();
  }

  @NonNull
  private final View.OnClickListener mFinishClickListener = (View v) ->
  {
    Statistics.INSTANCE.trackDownloaderDialogEvent(mAutoUpdate ?
                                                   DOWNLOADER_DIALOG_HIDE :
                                                   DOWNLOADER_DIALOG_LATER, 0);

    finish();
  };

  @NonNull
  private final View.OnClickListener mCancelClickListener = (View v) ->
  {
    Statistics.INSTANCE.trackDownloaderDialogEvent(DOWNLOADER_DIALOG_CANCEL,
                                                   mTotalSizeBytes / Constants.MB);

    MapManager.nativeCancel(CountryItem.getRootId());

    final UpdateInfo info = MapManager.nativeGetUpdateInfo(CountryItem.getRootId());
    if (info == null)
    {
      finish();
      return;
    }

    updateTotalSizes(info);

    mAutoUpdate = false;
    mOutdatedMaps = Framework.nativeGetOutdatedCountries();

    if (mStorageCallback != null)
      mStorageCallback.detach();

    mStorageCallback = new DetachableStorageCallback(this, mLeftoverMaps, mOutdatedMaps);
    mStorageCallback.attach(this);

    initViews();
  };

  @NonNull
  private final View.OnClickListener mUpdateClickListener = (View v) ->
      MapManager.warnOn3gUpdate(getActivity(), CountryItem.getRootId(), new Runnable()
      {
        @Override
        public void run()
        {
          mAutoUpdate = true;
          mFinishBtn.setText(getString(R.string.downloader_hide_screen));
          mTitle.setText(getString(R.string.whats_new_auto_update_updating_maps));
          setProgress(0, 0, mTotalSizeBytes);
          setCommonStatus(mProcessedMapId, mCommonStatusResId);
          MapManager.nativeUpdate(CountryItem.getRootId());
          UiUtils.show(mProgressBar, mInfo);
          UiUtils.hide(mUpdateBtn);

          Statistics.INSTANCE.trackDownloaderDialogEvent(DOWNLOADER_DIALOG_MANUAL_DOWNLOAD,
                                                         mTotalSizeBytes / Constants.MB);
        }
      });

  public static boolean showOn(@NonNull FragmentActivity activity,
                               @Nullable BaseNewsFragment.NewsDialogListener doneListener)
  {
    final FragmentManager fm = activity.getSupportFragmentManager();
    if (fm.isDestroyed())
      return false;

    final UpdateInfo info = MapManager.nativeGetUpdateInfo(CountryItem.getRootId());
    if (info == null)
      return false;

    @Framework.DoAfterUpdate final int result;

    Fragment f = fm.findFragmentByTag(UpdaterDialogFragment.class.getName());
    if (f != null)
    {
      ((UpdaterDialogFragment) f).mDoneListener = doneListener;
      return true;
    }
    else
    {
      result = Framework.nativeToDoAfterUpdate();
      if (result == Framework.DO_AFTER_UPDATE_MIGRATE || result == Framework.DO_AFTER_UPDATE_NOTHING)
        return false;

      Statistics.INSTANCE.trackDownloaderDialogEvent(DOWNLOADER_DIALOG_SHOW,
                                                     info.totalSize / Constants.MB);
    }

    final Bundle args = new Bundle();
    args.putBoolean(ARG_UPDATE_IMMEDIATELY, result == Framework.DO_AFTER_UPDATE_AUTO_UPDATE);
    args.putString(ARG_TOTAL_SIZE, StringUtils.getFileSizeString(info.totalSize));
    args.putLong(ARG_TOTAL_SIZE_BYTES, info.totalSize);
    args.putStringArray(ARG_OUTDATED_MAPS, Framework.nativeGetOutdatedCountries());

    final UpdaterDialogFragment fragment = new UpdaterDialogFragment();
    fragment.setArguments(args);
    fragment.mDoneListener = doneListener;
    FragmentTransaction transaction = fm.beginTransaction()
                                        .setCustomAnimations(android.R.anim.fade_in, android.R.anim.fade_out);
    fragment.show(transaction, UpdaterDialogFragment.class.getName());

    return true;
  }

  @Override
  protected int getCustomTheme()
  {
    return super.getFullscreenTheme();
  }

  @Override
  public void onCreate(@Nullable Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);
    if (savedInstanceState != null)
    {  // As long as we use HashSet to store leftover maps this cast is safe.
      //noinspection unchecked
      mLeftoverMaps = (HashSet<String>) savedInstanceState.getSerializable(EXTRA_LEFTOVER_MAPS);
      mProcessedMapId = savedInstanceState.getString(EXTRA_PROCESSED_MAP_ID);
      mCommonStatusResId = savedInstanceState.getInt(EXTRA_COMMON_STATUS_RES_ID);
      mAutoUpdate = savedInstanceState.getBoolean(ARG_UPDATE_IMMEDIATELY);
      mTotalSize = savedInstanceState.getString(ARG_TOTAL_SIZE);
      mTotalSizeBytes = savedInstanceState.getLong(ARG_TOTAL_SIZE_BYTES, 0L);
      mOutdatedMaps = savedInstanceState.getStringArray(ARG_OUTDATED_MAPS);
    }
    else
    {
      readArguments();
    }
    mStorageCallback = new DetachableStorageCallback(this, mLeftoverMaps, mOutdatedMaps);
  }

  @Override
  public void onSaveInstanceState(Bundle outState)
  {
    super.onSaveInstanceState(outState);
    outState.putSerializable(EXTRA_LEFTOVER_MAPS, mLeftoverMaps);
    outState.putString(EXTRA_PROCESSED_MAP_ID, mProcessedMapId);
    outState.putInt(EXTRA_COMMON_STATUS_RES_ID, mCommonStatusResId);
    outState.putBoolean(ARG_UPDATE_IMMEDIATELY, mAutoUpdate);
    outState.putString(ARG_TOTAL_SIZE, mTotalSize);
    outState.putLong(ARG_TOTAL_SIZE_BYTES, mTotalSizeBytes);
    outState.putStringArray(ARG_OUTDATED_MAPS, mOutdatedMaps);
  }

  @NonNull
  @Override
  public Dialog onCreateDialog(Bundle savedInstanceState)
  {
    Dialog res = super.onCreateDialog(savedInstanceState);
    res.requestWindowFeature(Window.FEATURE_NO_TITLE);

    View content = View.inflate(getActivity(), R.layout.fragment_updater, null);
    res.setContentView(content);

    mTitle = content.findViewById(R.id.title);
    mUpdateBtn = content.findViewById(R.id.update_btn);
    mProgressBar = content.findViewById(R.id.progress);
    mFinishBtn = content.findViewById(R.id.later_btn);
    mInfo = content.findViewById(R.id.info);
    mRelativeStatus = content.findViewById(R.id.relative_status);

    initViews();

    return res;
  }

  @Override
  public void onResume()
  {
    super.onResume();

    // The storage callback must be non-null at this point.
    //noinspection ConstantConditions
    mStorageCallback.attach(this);
    mProgressBar.setOnClickListener(mCancelClickListener);

    if (isAllUpdated() || Framework.nativeGetOutdatedCountries().length == 0)
    {
      finish();
      return;
    }

    if (mAutoUpdate)
    {
      if (!MapManager.nativeIsDownloading())
      {
        MapManager.warnOn3gUpdate(getActivity(), CountryItem.getRootId(), new Runnable()
        {
          @Override
          public void run()
          {
            MapManager.nativeUpdate(CountryItem.getRootId());
            Statistics.INSTANCE.trackDownloaderDialogEvent(DOWNLOADER_DIALOG_DOWNLOAD,
                                                           mTotalSizeBytes / Constants.MB);
          }
        });
      }
      else
      {
        final UpdateInfo info = MapManager.nativeGetUpdateInfo(CountryItem.getRootId());
        if (info == null)
        {
          finish();
          return;
        }

        updateTotalSizes(info);
        updateProgress();

        updateProcessedMapInfo();
        setCommonStatus(mProcessedMapId, mCommonStatusResId);
      }
    }
  }

  @Override
  public void onPause()
  {
    super.onPause();

    mProgressBar.setOnClickListener(null);

    if (mStorageCallback != null)
      mStorageCallback.detach();
  }

  @Override
  public void onDestroy()
  {
    super.onDestroy();
  }

  @Override
  public void onCancel(DialogInterface dialog)
  {
    if (MapManager.nativeIsDownloading())
      MapManager.nativeCancel(CountryItem.getRootId());

    if (mDoneListener != null)
      mDoneListener.onDialogDone();

    super.onCancel(dialog);
  }

  private void readArguments()
  {
    Bundle args = getArguments();
    if (args == null)
      return;

    mAutoUpdate = args.getBoolean(ARG_UPDATE_IMMEDIATELY);
    if (!mAutoUpdate && MapManager.nativeIsDownloading())
      mAutoUpdate = true;

    mTotalSize = args.getString(ARG_TOTAL_SIZE);
    mTotalSizeBytes = args.getLong(ARG_TOTAL_SIZE_BYTES, 0L);
    mOutdatedMaps = args.getStringArray(ARG_OUTDATED_MAPS);
    if (mLeftoverMaps == null && mOutdatedMaps != null && mOutdatedMaps.length > 0)
    {
      mLeftoverMaps = new HashSet<>(Arrays.asList(mOutdatedMaps));
    }
  }

  private void initViews()
  {
    UiUtils.showIf(mAutoUpdate, mProgressBar, mInfo);
    UiUtils.showIf(!mAutoUpdate, mUpdateBtn);

    mUpdateBtn.setText(getString(R.string.whats_new_auto_update_button_size, mTotalSize));
    mUpdateBtn.setOnClickListener(mUpdateClickListener);
    mFinishBtn.setText(mAutoUpdate ?
                       getString(R.string.downloader_hide_screen) :
                       getString(R.string.whats_new_auto_update_button_later));
    mFinishBtn.setOnClickListener(mFinishClickListener);
    mProgressBar.post(() -> mProgressBar.setPending(true));
    if (mAutoUpdate)
      setCommonStatus(mProcessedMapId, mCommonStatusResId);
    else
      mTitle.setText(getString(R.string.whats_new_auto_update_title));
  }

  private boolean isAllUpdated()
  {
    return mOutdatedMaps == null || mLeftoverMaps == null || mLeftoverMaps.isEmpty();
  }

  @NonNull
  String getRelativeStatusFormatted(int progress, long localSize, long remoteSize)
  {
    return getString(R.string.downloader_percent, progress + "%",
                     localSize / Constants.MB + getString(R.string.mb),
                     StringUtils.getFileSizeString(remoteSize));
  }

  void setProgress(int progress, long localSize, long remoteSize)
  {
    if (mProgressBar.isPending())
      mProgressBar.setPending(false);

    mProgressBar.setProgress(progress);
    mRelativeStatus.setText(getRelativeStatusFormatted(progress, localSize, remoteSize));
  }

  void setCommonStatus(@Nullable String mwmId, @StringRes int mwmStatusResId)
  {
    if (mwmId == null || mwmStatusResId == Utils.INVALID_ID)
      return;

    mProcessedMapId = mwmId;
    mCommonStatusResId = mwmStatusResId;

    String status = getString(mwmStatusResId, MapManager.nativeGetName(mwmId));
    mTitle.setText(status);
  }

  void updateTotalSizes(@NonNull UpdateInfo info)
  {
    mTotalSize = StringUtils.getFileSizeString(info.totalSize);
    mTotalSizeBytes = info.totalSize;
  }

  void updateProgress()
  {
    int progress = MapManager.nativeGetOverallProgress(mOutdatedMaps);
    setProgress(progress, mTotalSizeBytes * progress / 100, mTotalSizeBytes);
  }

  void updateProcessedMapInfo()
  {
    mProcessedMapId = MapManager.nativeGetCurrentDownloadingCountryId();

    if (mProcessedMapId == null)
      return;

    CountryItem processedCountryItem = new CountryItem(mProcessedMapId);
    MapManager.nativeGetAttributes(processedCountryItem);

    switch (processedCountryItem.status)
    {
      case CountryItem.STATUS_PROGRESS:
        mCommonStatusResId = R.string.downloader_process;
        break;
      case CountryItem.STATUS_APPLYING:
        mCommonStatusResId = R.string.downloader_applying;
        break;
      default:
        mCommonStatusResId = Utils.INVALID_ID;
        break;
    }
  }

  public long getTotalSizeBytes()
  {
    return mTotalSizeBytes;
  }

  private static class DetachableStorageCallback implements MapManager.StorageCallback
  {
    @Nullable
    private UpdaterDialogFragment mFragment;
    @Nullable
    private final Set<String> mLeftoverMaps;
    @Nullable
    private final String[] mOutdatedMaps;
    private int mListenerSlot = 0;

    DetachableStorageCallback(@Nullable UpdaterDialogFragment fragment,
                              @Nullable Set<String> leftoverMaps,
                              @Nullable String[] outdatedMaps)
    {
      mFragment = fragment;
      mLeftoverMaps = leftoverMaps;
      mOutdatedMaps = outdatedMaps;
    }

    @Override
    public void onStatusChanged(List<MapManager.StorageCallbackData> data)
    {
      String mwmId = null;
      @StringRes
      int mwmStatusResId = 0;
      for (MapManager.StorageCallbackData item : data)
      {
        if (!item.isLeafNode)
          continue;

        switch (item.newStatus)
        {
          case CountryItem.STATUS_FAILED:
            showErrorDialog(item);
            return;
          case CountryItem.STATUS_DONE:
            LOGGER.i(TAG, "Update finished for: " + item.countryId);
            if (mLeftoverMaps != null)
              mLeftoverMaps.remove(item.countryId);
            break;
          case CountryItem.STATUS_PROGRESS:
            mwmId = item.countryId;
            mwmStatusResId = R.string.downloader_process;
            break;
          case CountryItem.STATUS_APPLYING:
            mwmId = item.countryId;
            mwmStatusResId = R.string.downloader_applying;
            break;
          default:
            LOGGER.d(TAG, "Ignored status: " + item.newStatus +
                          ".For country: " + item.countryId);
            break;
        }
      }

      if (isFragmentAttached())
      {
        //noinspection ConstantConditions
        mFragment.setCommonStatus(mwmId, mwmStatusResId);
        if (mFragment.isAllUpdated())
          mFragment.finish();
      }
    }

    private void showErrorDialog(MapManager.StorageCallbackData item)
    {
      if (!isFragmentAttached())
        return;

      String text;
      switch (item.errorCode)
      {
        case CountryItem.ERROR_NO_INTERNET:
          //noinspection ConstantConditions
          text = mFragment.getString(R.string.common_check_internet_connection_dialog);
          break;

        case CountryItem.ERROR_OOM:
          //noinspection ConstantConditions
          text = mFragment.getString(R.string.downloader_no_space_title);
          break;

        default:
          text = String.valueOf(item.errorCode);
      }
      //noinspection ConstantConditions
      Statistics.INSTANCE.trackDownloaderDialogError(mFragment.getTotalSizeBytes() / Constants.MB,
                                                     text);
      MapManager.showErrorDialog(mFragment.getActivity(), item, new Utils.Proc<Boolean>()
      {
        @Override
        public void invoke(@NonNull Boolean result)
        {
          if (!isFragmentAttached())
            return;

          if (result)
          {
            MapManager.warnOn3gUpdate(mFragment.getActivity(), CountryItem.getRootId(), new Runnable()
            {
              @Override
              public void run()
              {
                MapManager.nativeUpdate(CountryItem.getRootId());
              }
            });
          }
          else
          {
            MapManager.nativeCancel(CountryItem.getRootId());
            mFragment.finish();
          }
        }
      });
    }

    @Override
    public void onProgress(String countryId, long localSizeBytes, long remoteSizeBytes)
    {
      if (mOutdatedMaps == null || !isFragmentAttached())
        return;

      int progress = MapManager.nativeGetOverallProgress(mOutdatedMaps);
      CountryItem root = new CountryItem(CountryItem.getRootId());
      MapManager.nativeGetAttributes(root);

      //noinspection ConstantConditions
      mFragment.setProgress(progress, root.downloadedBytes, root.bytesToDownload);
    }

    void attach(@NonNull UpdaterDialogFragment fragment)
    {
      mFragment = fragment;
      mListenerSlot = MapManager.nativeSubscribe(this);
    }

    void detach()
    {
      if (mFragment == null)
        throw new AssertionError("detach() should be called after attach() and only once");

      mFragment = null;
      MapManager.nativeUnsubscribe(mListenerSlot);
    }

    private boolean isFragmentAttached()
    {
      return mFragment != null && mFragment.isAdded();
    }
  }
}
