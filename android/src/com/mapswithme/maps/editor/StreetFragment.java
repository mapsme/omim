package com.mapswithme.maps.editor;

import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import com.mapswithme.maps.base.BaseMwmRecyclerFragment;
import com.mapswithme.maps.dialog.EditTextDialogFragment;
import com.mapswithme.maps.editor.data.LocalizedStreet;

public class StreetFragment extends BaseMwmRecyclerFragment implements EditTextDialogFragment.OnTextSaveListener
{
  private LocalizedStreet mSelectedString;

  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
  {
    mSelectedString = Editor.nativeGetStreet();
    return super.onCreateView(inflater, container, savedInstanceState);
  }

  @Override
  public void onSaveInstanceState(Bundle outState)
  {
    super.onSaveInstanceState(outState);
    Editor.nativeSetStreet(getStreet());
  }

  @Override
  protected RecyclerView.Adapter createAdapter()
  {
    return new StreetAdapter(this, Editor.nativeGetNearbyStreets(), mSelectedString);
  }

  @NonNull
  public LocalizedStreet getStreet()
  {
    return ((StreetAdapter) getAdapter()).getSelectedStreet();
  }

  @Override
  public void onSaveText(String text)
  {
    saveStreet(new LocalizedStreet(text, ""));
  }

  protected void saveStreet(LocalizedStreet street)
  {
    if (getParentFragment() instanceof EditorHostFragment)
      ((EditorHostFragment) getParentFragment()).setStreet(street);
  }
}
