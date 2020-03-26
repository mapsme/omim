package com.mapswithme.maps.widget.placepage;

import androidx.annotation.NonNull;
import com.github.mikephil.charting.charts.BarLineChartBase;
import com.github.mikephil.charting.formatter.DefaultValueFormatter;
import com.mapswithme.util.StringUtils;

public class AxisValueFormatter extends DefaultValueFormatter
{
  private static final String DEF_DIMEN = "m";
  private static final int DEF_DIGITS = 1;
  private static final int DISTANCE_FOR_METER_FORMAT = 1000;
  @NonNull
  private final BarLineChartBase mChart;

  public AxisValueFormatter(@NonNull BarLineChartBase chart)
  {
    super(DEF_DIGITS);
    mChart = chart;
  }

  @Override
  public String getFormattedValue(float value)
  {
    if (mChart.getVisibleXRange() > DISTANCE_FOR_METER_FORMAT)
      return StringUtils.nativeFormatDistance(value);

    return (int) value + " " + DEF_DIMEN;
  }
}
