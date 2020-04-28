package com.mapswithme.maps.maplayer;

import android.content.Context;
import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.StringRes;

import com.mapswithme.maps.R;
import com.mapswithme.util.ThemeUtils;

public abstract class BottomSheetItem
{
  @DrawableRes
  private final int mEnabledStateDrawableResId;
  @DrawableRes
  private final int mDisabledStateDrawableResId;
  @StringRes
  private final int mTitleResId;
  @NonNull
  private final Mode mMode;

  BottomSheetItem(@DrawableRes int enabledStateDrawableResId,
                  @DrawableRes int disabledStateDrawableResId,
                  @StringRes int titleResId,
                  @NonNull Mode mode)
  {
    mEnabledStateDrawableResId = enabledStateDrawableResId;
    mDisabledStateDrawableResId = disabledStateDrawableResId;
    mTitleResId = titleResId;
    mMode = mode;
  }

  @NonNull
  public Mode getMode()
  {
    return mMode;
  }

  @DrawableRes
  public int getEnabledStateDrawable()
  {
    return mEnabledStateDrawableResId;
  }

  @DrawableRes
  public int getDisabledStateDrawable()
  {
    return mDisabledStateDrawableResId;
  }

  @StringRes
  public int getTitle()
  {
    return mTitleResId;
  }

  public static class Subway extends BottomSheetItem
  {
    private Subway(int drawableResId, int disabledStateDrawableResId)
    {
      super(drawableResId, disabledStateDrawableResId, R.string.button_layer_subway, Mode.SUBWAY);
    }

    public static BottomSheetItem makeInstance(@NonNull Context mContext)
    {
      int disabled = ThemeUtils.getResource(mContext, R.attr.subwayMenuDisabled);
      return new Subway(R.drawable.ic_subway_menu_on, disabled);
    }
  }

  public static class Traffic extends BottomSheetItem
  {
    private Traffic(int drawableResId, int disabledStateDrawableResId)
    {
      super(drawableResId, disabledStateDrawableResId, R.string.button_layer_traffic, Mode.TRAFFIC);
    }

    public static BottomSheetItem makeInstance(@NonNull Context mContext)
    {
      int disabled = ThemeUtils.getResource(mContext, R.attr.trafficMenuDisabled);

      return new Traffic(R.drawable.ic_traffic_menu_on, disabled);
    }
  }

  public static class Isolines extends BottomSheetItem
  {
    private Isolines(int drawableResId, int disabledStateDrawableResId)
    {
      super(drawableResId, disabledStateDrawableResId, R.string.button_layer_isolines, Mode.ISOLINES);
    }

    public static BottomSheetItem makeInstance(@NonNull Context mContext)
    {
      int disabled = ThemeUtils.getResource(mContext, R.attr.isoLinesMenuDisabled);

      return new Isolines(R.drawable.ic_planning_route_isomaps_dark_on, disabled);
    }
  }

  public static class Guides extends BottomSheetItem
  {
    private Guides(int drawableResId, int disabledStateDrawableResId)
    {
      super(drawableResId, disabledStateDrawableResId, R.string.button_layer_guides, Mode.GUIDES);
    }

    public static BottomSheetItem makeInstance(@NonNull Context mContext)
    {
      int disabled = ThemeUtils.getResource(mContext, R.attr.guidesMenuDisabled);

      return new Guides(R.drawable.guides_layer_active_light, disabled);
    }
  }
}
