package com.mapswithme.maps.editor;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;

import com.mapswithme.maps.Framework;
import com.mapswithme.maps.base.BaseMwmFragmentActivity;
import com.mapswithme.maps.bookmarks.data.MapObject;
import com.mapswithme.maps.editor.data.FeatureCategory;

import static com.mapswithme.maps.editor.FeatureCategoryActivity.EXTRA_FEATURE_CATEGORY;
import static com.mapswithme.maps.editor.FeatureCategoryActivity.EXTRA_FEATURE_POSITION;

public class EditorActivity extends BaseMwmFragmentActivity
{
  public static final String EXTRA_NEW_OBJECT = "ExtraNewMapObject";
  private static final String EXTRA_MAP_OBJECT = "ExtraMapObject";
  public static final String EXTRA_DRAW_SCALE = "ExtraDrawScale";

  private int mDrawScale = -1;

  @Override
  protected Class<? extends Fragment> getFragmentClass()
  {
    return EditorHostFragment.class;
  }

  public static void start(@NonNull Activity activity, @NonNull MapObject mapObject, int drawScale)
  {
    final Intent intent = new Intent(activity, EditorActivity.class);
    intent.putExtra(EXTRA_MAP_OBJECT, mapObject);
    intent.putExtra(EXTRA_DRAW_SCALE, drawScale);
    activity.startActivity(intent);
  }

  @Override
  protected void onCreate(@Nullable Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);

    Bundle extras = getIntent().getExtras();
    if (extras != null)
    {
      mDrawScale = extras.getInt(EXTRA_DRAW_SCALE);

      boolean newObject = extras.getBoolean(EditorActivity.EXTRA_NEW_OBJECT, false);
      if (newObject)
      {
        FeatureCategory category = extras.getParcelable(EXTRA_FEATURE_CATEGORY);
        double[] position = extras.getDoubleArray(EXTRA_FEATURE_POSITION);
        if (category != null && position != null)
          Editor.createMapObject(category, position[0], position[1], mDrawScale);
      }

      MapObject mapObject = extras.getParcelable(EXTRA_MAP_OBJECT);
      if (mapObject != null && !newObject)
        Framework.nativeRestoreMapObject(mapObject);
    }
  }

  public int getDrawScale()
  {
    return mDrawScale;
  }
}
