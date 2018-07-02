package com.mapswithme.maps.subway;

import android.support.annotation.MainThread;
import android.support.annotation.NonNull;
import android.widget.Toast;

import com.mapswithme.maps.MwmApplication;
import com.mapswithme.maps.R;
import com.mapswithme.maps.content.AbstractContextualListener;

public interface OnSubwaySchemeChangedListener
{
  @SuppressWarnings("unused")
  @MainThread
  void onTransitStateChanged(int type);

  class Default extends AbstractContextualListener implements OnSubwaySchemeChangedListener
  {
    public Default(@NonNull MwmApplication app)
    {
      super(app);
    }

    @Override
    public void onTransitStateChanged(int index)
    {
      MwmApplication app = getApp();
      if (app == null)
        return;

      SubwaySchemeState state = SubwaySchemeState.makeInstance(index);
      if (state != SubwaySchemeState.NO_DATA)
        return;

      Toast.makeText(app, R.string.subway_data_unavailable, Toast.LENGTH_SHORT).show();
    }
  }
}
