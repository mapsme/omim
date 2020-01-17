package com.mapswithme.maps.maplayer;

import android.content.Context;

import androidx.annotation.NonNull;

import com.mapswithme.maps.maplayer.isolines.IsolinesManager;
import com.mapswithme.maps.maplayer.subway.SubwayManager;
import com.mapswithme.maps.maplayer.traffic.TrafficManager;

public enum Mode
{
  TRAFFIC
      {
        @Override
        public boolean isEnabled(@NonNull Context context)
        {
          return !SubwayManager.from(context).isEnabled()
                 && TrafficManager.INSTANCE.isEnabled();
        }

        @Override
        public void setEnabled(@NonNull Context context, boolean isEnabled)
        {
          TrafficManager.INSTANCE.setEnabled(isEnabled);
        }

        @Override
        public void toggle(@NonNull Context context)
        {
          TrafficManager.INSTANCE.toggle();
          SubwayManager.from(context).setEnabled(false);
          IsolinesManager.from(context).setEnabled(false);
        }
      },
  SUBWAY
      {
        @Override
        public boolean isEnabled(@NonNull Context context)
        {
          return SubwayManager.from(context).isEnabled();
        }

        @Override
        public void setEnabled(@NonNull Context context, boolean isEnabled)
        {
          SubwayManager.from(context).setEnabled(isEnabled);
        }

        @Override
        public void toggle(@NonNull Context context)
        {
          SubwayManager.from(context).toggle();
          TrafficManager.INSTANCE.setEnabled(false);
          IsolinesManager.from(context).setEnabled(false);
        }
      },

  ISOLINES
      {
        @Override
        public boolean isEnabled(@NonNull Context context)
        {
          return IsolinesManager.from(context).isEnabled();
        }

        @Override
        public void setEnabled(@NonNull Context context, boolean isEnabled)
        {
          IsolinesManager.from(context).setEnabled(isEnabled);
        }

        @Override
        public void toggle(@NonNull Context context)
        {
          IsolinesManager.from(context).toggle();
          TrafficManager.INSTANCE.setEnabled(false);
          SubwayManager.from(context).setEnabled(false);
        }
      };

  public abstract boolean isEnabled(@NonNull Context context);

  public abstract void setEnabled(@NonNull Context context, boolean isEnabled);

  public abstract void toggle(@NonNull Context context);
}
