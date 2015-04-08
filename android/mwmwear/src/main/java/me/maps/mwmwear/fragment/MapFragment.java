package me.maps.mwmwear.fragment;

import android.app.Fragment;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import me.maps.mwmwear.R;

public class MapFragment extends Fragment
{
  private ImageView mIvMap;

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
  {
    mIvMap = (ImageView) inflater.inflate(R.layout.fragment_map, container, false);
    return mIvMap;
  }

  public void setBitmap(Bitmap bitmap)
  {
    mIvMap.setImageBitmap(bitmap);
  }
}
