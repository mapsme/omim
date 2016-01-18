package com.mapswithme.maps.editor;

import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import com.mapswithme.maps.R;
import com.mapswithme.maps.base.BaseMwmToolbarFragment;
import com.mapswithme.maps.base.OnBackPressListener;
import com.mapswithme.maps.bookmarks.data.MapObject;
import com.mapswithme.maps.bookmarks.data.Metadata;
import com.mapswithme.maps.widget.placepage.TimetableFragment;


public class EditorHostFragment extends BaseMwmToolbarFragment
                             implements OnBackPressListener, View.OnClickListener
{
  public static final String EXTRA_MAP_OBJECT = "MapObject";

  public static final int MODE_MAPOBJECT = 0;
  public static final int MODE_OPENING_HOURS = 1;
  public static final int MODE_STREET = 2;
  public static final int MODE_CUISINE = 3;

  private int mMode = MODE_MAPOBJECT;

  private MapObject mEditedObject;

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState)
  {
    return inflater.inflate(R.layout.fragment_editor_host, container, false);
  }

  @Override
  public void onViewCreated(View view, @Nullable Bundle savedInstanceState)
  {
    super.onViewCreated(view, savedInstanceState);
    mEditedObject = getArguments().getParcelable(EditorHostFragment.EXTRA_MAP_OBJECT);
    editMapObject();
    mToolbarController.findViewById(R.id.save).setOnClickListener(this);

    mToolbarController.getToolbar().setNavigationOnClickListener(new View.OnClickListener() {
      @Override
      public void onClick(View v)
      {
        onBackPressed();
      }
    });
  }

  @Override
  public boolean onBackPressed()
  {
    switch (mMode)
    {
    case MODE_OPENING_HOURS:
    case MODE_STREET:
    case MODE_CUISINE:
      editMapObject();
      return true;
    }
    return false;
  }

  protected void editMapObject()
  {
    mMode = MODE_MAPOBJECT;
    mToolbarController.setTitle("Edit POI");
    final Bundle args = new Bundle();
    args.putParcelable(EXTRA_MAP_OBJECT, mEditedObject);
    final Fragment editorFragment = Fragment.instantiate(getActivity(), EditorFragment.class.getName(), args);
    getChildFragmentManager().beginTransaction()
                             .replace(R.id.fragment_container, editorFragment, EditorFragment.class.getName())
                             .commit();
  }

  protected void editTimetable()
  {
    mMode = MODE_OPENING_HOURS;
    mToolbarController.setTitle("Opening hours");
    final Bundle args = new Bundle();
    args.putString(TimetableFragment.EXTRA_TIME, mEditedObject.getMetadata(Metadata.MetadataType.FMD_OPEN_HOURS));
    final Fragment editorFragment = Fragment.instantiate(getActivity(), TimetableFragment.class.getName(), args);
    getChildFragmentManager().beginTransaction()
                             .replace(R.id.fragment_container, editorFragment, TimetableFragment.class.getName())
                             .commit();
  }

  protected void editStreet()
  {
    mMode = MODE_STREET;
    // TODO choose street
  }

  protected void editCuisine()
  {
    mMode = MODE_CUISINE;
    // TODO choose cuisine
  }

  @Override
  public void onClick(View v)
  {
    switch (v.getId())
    {
    case R.id.save:
      switch (mMode)
      {
      case MODE_OPENING_HOURS:
        final TimetableFragment fragment = (TimetableFragment) getChildFragmentManager().findFragmentByTag(TimetableFragment.class.getName());
        mEditedObject.addMetadata(Metadata.MetadataType.FMD_OPEN_HOURS.toInt(), fragment.getTimetable());
        editMapObject();
        break;
      case MODE_STREET:
        // get street
        break;
      case MODE_CUISINE:
        // get cuisine
        break;
      case MODE_MAPOBJECT:
        final EditorFragment editorFragment = (EditorFragment) getChildFragmentManager().findFragmentByTag(EditorFragment.class.getName());
        Editor.nativeSetMetadata(Metadata.MetadataType.FMD_PHONE_NUMBER.toInt(), editorFragment.getPhone());
        Editor.nativeSetMetadata(Metadata.MetadataType.FMD_WEBSITE.toInt(), editorFragment.getWebsite());
        Editor.nativeSetMetadata(Metadata.MetadataType.FMD_EMAIL.toInt(), editorFragment.getEmail());
        Editor.nativeSetMetadata(Metadata.MetadataType.FMD_CUISINE.toInt(), editorFragment.getCuisine());
        Editor.nativeSetMetadata(Metadata.MetadataType.FMD_INTERNET.toInt(), editorFragment.getWifi());
        Editor.nativeSetName(editorFragment.getName());
        Editor.nativeEditFeature(editorFragment.getStreet(), editorFragment.getHouseNumber());
        break;
      }
      break;
    }
  }
}
