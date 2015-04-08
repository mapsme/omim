package me.maps.mwmwear.fragment;

import android.app.Fragment;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import me.maps.mwmwear.R;

public class VoiceFragment extends Fragment
{
  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
  {
    final ViewGroup root = (ViewGroup) inflater.inflate(R.layout.activity_main, container,  false);
    return root;
  }
}
