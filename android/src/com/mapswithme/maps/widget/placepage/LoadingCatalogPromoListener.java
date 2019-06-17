package com.mapswithme.maps.widget.placepage;

import android.app.Activity;
import android.support.annotation.NonNull;

import com.mapswithme.maps.gallery.AbstractActionButtonListener;
import com.mapswithme.maps.gallery.Items;
import com.mapswithme.util.Utils;

public class LoadingCatalogPromoListener<T extends Items.Item> extends AbstractActionButtonListener<T>
{
  public LoadingCatalogPromoListener(@NonNull Activity activity)
  {
    super(activity);
  }

  @Override
  public void onItemSelected(@NonNull T item, int position)
  {
    Utils.openUrl(getActivity(), item.getUrl());
  }
}
