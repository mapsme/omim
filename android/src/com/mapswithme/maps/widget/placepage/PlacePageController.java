package com.mapswithme.maps.widget.placepage;

import android.app.Application;
import android.os.Bundle;
import androidx.annotation.NonNull;

import com.mapswithme.maps.base.Initializable;
import com.mapswithme.maps.base.Savable;
import com.mapswithme.maps.bookmarks.data.MapObject;

public interface PlacePageController extends Initializable, Savable<Bundle>,
                                             Application.ActivityLifecycleCallbacks
{
  void openFor(@NonNull MapObject object);
  void close();
  boolean isClosed();

  interface SlideListener
  {
    void onPlacePageSlide(int top);
  }
}
