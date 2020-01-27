package com.mapswithme.maps.maplayer.traffic;

import android.text.TextUtils;

import androidx.annotation.MainThread;
import androidx.annotation.NonNull;
import com.mapswithme.util.statistics.StatisticValueConverter;
import com.mapswithme.util.statistics.Statistics;

import java.util.List;

enum TrafficState implements StatisticValueConverter<String>
{
  DISABLED
      {
        @Override
        protected void activateInternal(@NonNull TrafficManager.TrafficCallback callback)
        {
          callback.onDisabled();
        }
      },

  ENABLED
      {
        @Override
        protected void activateInternal(@NonNull TrafficManager.TrafficCallback callback)
        {
          callback.onEnabled();
        }
      },

  WAITING_DATA
      {
        @Override
        protected void activateInternal(@NonNull TrafficManager.TrafficCallback callback)
        {
          callback.onWaitingData();
        }
      },

  OUTDATED
      {
        @Override
        protected void activateInternal(@NonNull TrafficManager.TrafficCallback callback)
        {
          callback.onOutdated();
        }
      },

  NO_DATA
      {
        @Override
        protected void activateInternal(@NonNull TrafficManager.TrafficCallback callback)
        {
          callback.onNoData();
        }

        @NonNull
        @Override
        public String toStatisticValue()
        {
          return Statistics.ParamValue.UNAVAILABLE;
        }
      },

  NETWORK_ERROR
      {
        @Override
        protected void activateInternal(@NonNull TrafficManager.TrafficCallback callback)
        {
          callback.onNetworkError();
        }

        @NonNull
        @Override
        public String toStatisticValue()
        {
          return Statistics.EventParam.ERROR;
        }
      },

  EXPIRED_DATA
      {
        @Override
        protected void activateInternal(@NonNull TrafficManager.TrafficCallback callback)
        {
          callback.onExpiredData();
        }
      },

  EXPIRED_APP
      {
        @Override
        protected void activateInternal(@NonNull TrafficManager.TrafficCallback callback)
        {
          callback.onExpiredApp();
        }
      };

  @NonNull
  @Override
  public String toStatisticValue()
  {
    return "";
  }

  public void activate(@NonNull List<TrafficManager.TrafficCallback> trafficCallbacks)
  {
    for (TrafficManager.TrafficCallback callback : trafficCallbacks)
      activateInternal(callback);

    if (!TextUtils.isEmpty(toStatisticValue()))
      Statistics.INSTANCE.trackTrafficEvent(toStatisticValue());
  }

  protected abstract void activateInternal(@NonNull TrafficManager.TrafficCallback callback);

  interface StateChangeListener
  {
    // This method is called from JNI layer.
    @SuppressWarnings("unused")
    @MainThread
    void onTrafficStateChanged(int state);
  }

  @MainThread
  static native void nativeSetListener(@NonNull StateChangeListener listener);

  static native void nativeRemoveListener();

  static native void nativeEnable();

  static native void nativeDisable();

  static native boolean nativeIsEnabled();
}
