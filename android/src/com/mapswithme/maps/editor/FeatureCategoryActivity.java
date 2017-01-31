package com.mapswithme.maps.editor;

import android.content.Intent;
import android.os.Bundle;
import android.support.v4.app.Fragment;

import com.mapswithme.maps.Framework;
import com.mapswithme.maps.base.BaseMwmFragmentActivity;
import com.mapswithme.maps.editor.data.FeatureCategory;

import static com.mapswithme.maps.editor.EditorActivity.EXTRA_DRAW_SCALE;

public class FeatureCategoryActivity extends BaseMwmFragmentActivity implements FeatureCategoryFragment.FeatureCategoryListener
{
  public static final String EXTRA_FEATURE_CATEGORY = "FeatureCategory";
  public static final String EXTRA_FEATURE_POSITION = "FeaturePosition";

  @Override
  protected void onCreate(Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);
  }

  @Override
  protected Class<? extends Fragment> getFragmentClass()
  {
    return FeatureCategoryFragment.class;
  }

  @Override
  public void onFeatureCategorySelected(FeatureCategory category)
  {
    //Editor.createMapObject(category);
    final Intent intent = new Intent(this, EditorActivity.class);
    intent.putExtra(EXTRA_FEATURE_CATEGORY, category);
    intent.putExtra(EditorActivity.EXTRA_NEW_OBJECT, true);
    intent.putExtra(EXTRA_FEATURE_POSITION, Framework.nativeGetScreenRectCenterMercator());
    intent.putExtra(EXTRA_DRAW_SCALE, Framework.nativeGetDrawScale());
    startActivity(intent);
  }
}
