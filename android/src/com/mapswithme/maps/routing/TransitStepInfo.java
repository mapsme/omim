package com.mapswithme.maps.routing;

import android.support.annotation.IntDef;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Represents TransitStepInfo from core.
 */
public class TransitStepInfo
{
  private static final int TRANSIT_TYPE_INTERMEDIATE_POINT = 0;
  private static final int TRANSIT_TYPE_PEDESTRIAN = 1;
  private static final int TRANSIT_TYPE_SUBWAY = 2;
  private static final int TRANSIT_TYPE_TRAIN = 3;
  private static final int TRANSIT_TYPE_LIGHT_RAIL = 4;
  private static final int TRANSIT_TYPE_MONORAIL = 5;

  @Retention(RetentionPolicy.SOURCE)
  @IntDef({ TRANSIT_TYPE_INTERMEDIATE_POINT, TRANSIT_TYPE_PEDESTRIAN, TRANSIT_TYPE_SUBWAY,
            TRANSIT_TYPE_TRAIN, TRANSIT_TYPE_LIGHT_RAIL, TRANSIT_TYPE_MONORAIL })
  @interface TransitType {}

  @NonNull
  private final TransitStepType mType;
  @Nullable
  private final String mDistance;
  @Nullable
  private final String mDistanceUnits;
  private final int mTimeInSec;
  @Nullable
  private final String mNumber;
  private final int mColor;
  private final int mIntermediateIndex;

  TransitStepInfo(@TransitType int type, @Nullable String distance, @Nullable String distanceUnits,
                  int timeInSec, @Nullable String number, int color, int intermediateIndex)
  {
    mType = TransitStepType.values()[type];
    mDistance = distance;
    mDistanceUnits = distanceUnits;
    mTimeInSec = timeInSec;
    mNumber = number;
    mColor = color;
    mIntermediateIndex = intermediateIndex;
  }

  @NonNull
  public TransitStepType getType()
  {
    return mType;
  }

  @Nullable
  public String getDistance()
  {
    return mDistance;
  }

  @Nullable
  public String getDistanceUnits()
  {
    return mDistanceUnits;
  }

  public int getTimeInSec()
  {
    return mTimeInSec;
  }

  @Nullable
  public String getNumber()
  {
    return mNumber;
  }

  public int getColor()
  {
    return mColor;
  }

  public int getIntermediateIndex()
  {
    return mIntermediateIndex;
  }
}
