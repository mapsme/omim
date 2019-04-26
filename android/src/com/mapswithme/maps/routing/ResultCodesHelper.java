package com.mapswithme.maps.routing;

import android.content.res.Resources;
import android.support.annotation.NonNull;
import android.util.Pair;

import com.mapswithme.maps.MwmApplication;
import com.mapswithme.maps.R;
import com.mapswithme.maps.location.LocationHelper;
import com.mapswithme.maps.location.LocationState;

import java.util.ArrayList;
import java.util.List;

class ResultCodesHelper
{
  // Codes correspond to native routing::RouterResultCode in routing/routing_callbacks.hpp
  public enum ResultCode
  {
    NO_ERROR
        {
          @Override
          public void processEvent(@NonNull RoutingController controller)
          {
            controller.updatePlan();
          }
        },
    CANCELLED
        {
          @Override
          public void processEvent(@NonNull RoutingController controller)
          {
            controller.setBuildState(RoutingController.BuildState.NONE);
            controller.updatePlan();
          }
        },

    NO_POSITION,
    INCONSISTENT_MWM_ROUTE,
    ROUTING_FILE_NOT_EXIST,
    START_POINT_NOT_FOUND,
    END_POINT_NOT_FOUND,
    DIFFERENT_MWM,
    ROUTE_NOT_FOUND,
    NEED_MORE_MAPS
        {
          @Override
          protected void invalidateProgress(@NonNull RoutingController controller)
          {
            /* Do nothing */
          }
        },
    INTERNAL_ERROR,
    FILE_TOO_OLD,
    INTERMEDIATE_POINT_NOT_FOUND,
    TRANSIT_ROUTE_NOT_FOUND_NO_NETWORK,
    TRANSIT_ROUTE_NOT_FOUND_TOO_LONG_PEDESTRIAN,
    ROUTE_NOT_FOUND_REDRESS_ROUTE_ERROR,
    HAS_WARNINGS
        {
          @Override
          public void processEvent(@NonNull RoutingController controller)
          {
            controller.updatePlan();
            if (RoutingOptions.hasAnyOptions())
              controller.showRouteWarningDialog();
          }
        };

    public void processEvent(@NonNull RoutingController controller)
    {
      invalidateProgress(controller);
      showDialog(controller);
    }

    protected void invalidateProgress(@NonNull RoutingController controller)
    {
      controller.invalidateBuildProgress();
    }

    private void showDialog(@NonNull RoutingController controller)
    {
      controller.showErrorRoutingDialog();
    }
  }

  static Pair<String, String> getDialogTitleSubtitle(int errorCode, int missingCount)
  {
    Resources resources = MwmApplication.get().getResources();
    int titleRes = 0;
    List<String> messages = new ArrayList<>();
    ResultCode code = ResultCode.values()[errorCode];
    switch (code)
    {
    case NO_POSITION:
      if (LocationHelper.INSTANCE.getMyPositionMode() == LocationState.NOT_FOLLOW_NO_POSITION)
      {
        titleRes = R.string.dialog_routing_location_turn_on;
        messages.add(resources.getString(R.string.dialog_routing_location_unknown_turn_on));
      }
      else
      {
        titleRes = R.string.dialog_routing_check_gps;
        messages.add(resources.getString(R.string.dialog_routing_error_location_not_found));
        messages.add(resources.getString(R.string.dialog_routing_location_turn_wifi));
      }
      break;
    case INCONSISTENT_MWM_ROUTE:
    case ROUTING_FILE_NOT_EXIST:
      titleRes = R.string.routing_download_maps_along;
      messages.add(resources.getString(R.string.routing_requires_all_map));
      break;
    case START_POINT_NOT_FOUND:
      titleRes = R.string.dialog_routing_change_start;
      messages.add(resources.getString(R.string.dialog_routing_start_not_determined));
      messages.add(resources.getString(R.string.dialog_routing_select_closer_start));
      break;
    case END_POINT_NOT_FOUND:
      titleRes = R.string.dialog_routing_change_end;
      messages.add(resources.getString(R.string.dialog_routing_end_not_determined));
      messages.add(resources.getString(R.string.dialog_routing_select_closer_end));
      break;
    case INTERMEDIATE_POINT_NOT_FOUND:
      titleRes = R.string.dialog_routing_change_intermediate;
      messages.add(resources.getString(R.string.dialog_routing_intermediate_not_determined));
      break;
    case DIFFERENT_MWM:
      messages.add(resources.getString(R.string.routing_failed_cross_mwm_building));
      break;
    case FILE_TOO_OLD:
      titleRes = R.string.downloader_update_maps;
      messages.add(resources.getString(R.string.downloader_mwm_migration_dialog));
      break;
    case TRANSIT_ROUTE_NOT_FOUND_NO_NETWORK:
      messages.add(resources.getString(R.string.transit_not_found));
      break;
    case TRANSIT_ROUTE_NOT_FOUND_TOO_LONG_PEDESTRIAN:
      messages.add(resources.getString(R.string.dialog_pedestrian_route_is_long));
      break;
    case ROUTE_NOT_FOUND:
    case ROUTE_NOT_FOUND_REDRESS_ROUTE_ERROR:
      if (missingCount == 0)
      {
        titleRes = R.string.dialog_routing_unable_locate_route;
        messages.add(resources.getString(R.string.dialog_routing_cant_build_route));
        messages.add(resources.getString(R.string.dialog_routing_change_start_or_end));
      }
      else
      {
        titleRes = R.string.routing_download_maps_along;
        messages.add(resources.getString(R.string.routing_requires_all_map));
      }
      break;
    case INTERNAL_ERROR:
      titleRes = R.string.dialog_routing_system_error;
      messages.add(resources.getString(R.string.dialog_routing_application_error));
      messages.add(resources.getString(R.string.dialog_routing_try_again));
      break;
    case NEED_MORE_MAPS:
      titleRes = R.string.dialog_routing_download_and_build_cross_route;
      messages.add(resources.getString(R.string.dialog_routing_download_cross_route));
      break;
    }

    StringBuilder builder = new StringBuilder();
    for (String messagePart : messages)
    {
      if (builder.length() > 0)
        builder.append("\n\n");

      builder.append(messagePart);
    }

    return new Pair<>(titleRes == 0 ? "" : resources.getString(titleRes), builder.toString());
  }

  static boolean isDownloadable(int resultCode, int missingCount)
  {
    if (missingCount <= 0)
      return false;

    ResultCode code = ResultCode.values()[resultCode];
    switch (code)
    {
      case INCONSISTENT_MWM_ROUTE:
      case ROUTING_FILE_NOT_EXIST:
      case NEED_MORE_MAPS:
      case ROUTE_NOT_FOUND:
      case FILE_TOO_OLD:
        return true;
    }

    return false;
  }

  static boolean isMoreMapsNeeded(@NonNull ResultCode resultCode)
  {
    return resultCode == ResultCode.NEED_MORE_MAPS;
  }
}
