package com.mapswithme.maps.routing;

import android.content.DialogInterface;
import android.os.Bundle;
import androidx.fragment.app.Fragment;
import androidx.appcompat.app.AlertDialog;
import android.view.View;
import android.view.ViewGroup;

import java.util.HashSet;
import java.util.List;
import java.util.Set;

import com.mapswithme.maps.MwmApplication;
import com.mapswithme.maps.R;
import com.mapswithme.maps.downloader.CountryItem;
import com.mapswithme.maps.downloader.MapManager;
import com.mapswithme.maps.widget.WheelProgressView;
import com.mapswithme.util.UiUtils;

public class RoutingMapsDownloadFragment extends BaseRoutingErrorDialogFragment
{
  private ViewGroup mItemsFrame;

  private final Set<String> mMaps = new HashSet<>();

  private int mSubscribeSlot;

  @Override
  void beforeDialogCreated(AlertDialog.Builder builder)
  {
    super.beforeDialogCreated(builder);
    builder.setTitle(R.string.downloading);

    mMapsArray = new String[mMissingMaps.size()];
    for (int i = 0; i < mMissingMaps.size(); i++)
    {
      CountryItem item = mMissingMaps.get(i);
      mMaps.add(item.id);
      mMapsArray[i] = item.id;
    }

    for (String map : mMaps)
      MapManager.nativeDownload(map);
  }

  private View setupFrame(View frame)
  {
    UiUtils.hide(frame.findViewById(R.id.tv__message));
    mItemsFrame = (ViewGroup) frame.findViewById(R.id.items_frame);
    return frame;
  }

  @Override
  View buildSingleMapView(CountryItem map)
  {
    View res = setupFrame(super.buildSingleMapView(map));
    bindGroup(res);
    return res;
  }

  @Override
  View buildMultipleMapView()
  {
    return setupFrame(super.buildMultipleMapView());
  }

  private WheelProgressView getWheel()
  {
    if (mItemsFrame == null)
      return null;

    View frame = mItemsFrame.getChildAt(0);
    if (frame == null)
      return null;

    WheelProgressView res = (WheelProgressView) frame.findViewById(R.id.wheel_progress);
    return ((res != null && UiUtils.isVisible(res)) ? res : null);
  }

  private void updateWheel(WheelProgressView wheel)
  {
    int progress = MapManager.nativeGetOverallProgress(mMapsArray);
    if (progress == 0)
      wheel.setPending(true);
    else
    {
      wheel.setPending(false);
      wheel.setProgress(progress);
    }
  }

  @Override
  void bindGroup(View view)
  {
    WheelProgressView wheel = (WheelProgressView) view.findViewById(R.id.wheel_progress);
    UiUtils.show(wheel);
    updateWheel(wheel);
  }

  @Override
  public void onDismiss(DialogInterface dialog)
  {
    if (mCancelled)
      for (String item : mMaps)
        MapManager.nativeCancel(item);

    super.onDismiss(dialog);
  }

  @Override
  public void onStart()
  {
    super.onStart();
    setCancelable(false);

    mSubscribeSlot = MapManager.nativeSubscribe(new MapManager.StorageCallback()
    {
      {
        update();
      }

      private void update()
      {
        WheelProgressView wheel = getWheel();
        if (wheel != null)
          updateWheel(wheel);
      }

      @Override
      public void onStatusChanged(List<MapManager.StorageCallbackData> data)
      {
        for (MapManager.StorageCallbackData item : data)
          if (mMaps.contains(item.countryId))
          {
            if (item.newStatus == CountryItem.STATUS_DONE)
            {
              mMaps.remove(item.countryId);
              if (mMaps.isEmpty())
              {
                mCancelled = false;
                RoutingController.get().checkAndBuildRoute();
                dismissAllowingStateLoss();
                return;
              }
            }

            update();
            return;
          }
      }

      @Override
      public void onProgress(String countryId, long localSize, long remoteSize)
      {
        if (mMaps.contains(countryId))
          update();
      }
    });
  }

  @Override
  public void onStop()
  {
    super.onStop();

    if (mSubscribeSlot != 0)
    {
      MapManager.nativeUnsubscribe(mSubscribeSlot);
      mSubscribeSlot = 0;
    }
  }

  public static RoutingMapsDownloadFragment create(String[] missingMaps)
  {
    Bundle args = new Bundle();
    args.putStringArray(EXTRA_MISSING_MAPS, missingMaps);
    RoutingMapsDownloadFragment res = (RoutingMapsDownloadFragment) Fragment.instantiate(MwmApplication.get(), RoutingMapsDownloadFragment.class.getName());
    res.setArguments(args);
    return res;
  }
}
