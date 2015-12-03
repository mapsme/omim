package com.mapswithme.maps.editor.data;

import android.support.annotation.NonNull;

public class Timetable
{
  public final Timespan mWorkingTimespan;
  public final Timespan[] mClosedTimespans;
  public final boolean mIsFullday;
  public final int[] mWeekdays;

  public Timetable(String time)
  {
    // FIXME initialize correctly with string
    mWorkingTimespan = new Timespan(new HoursMinutes(24, 0), new HoursMinutes(0, 0));
    mClosedTimespans = new Timespan[0];
    mWeekdays = new int[0];
    mIsFullday = false;
  }

  public Timetable(@NonNull Timespan workingTime, @NonNull Timespan closedHours, boolean isFullday, @NonNull int[] weekdays)
  {
    this(workingTime, new Timespan[]{closedHours}, isFullday, weekdays);
  }

  public Timetable(@NonNull Timespan workingTime, @NonNull Timespan[] closedHours, boolean isFullday, @NonNull int[] weekdays)
  {
    mWorkingTimespan = workingTime;
    mClosedTimespans = closedHours;
    mIsFullday = isFullday;
    mWeekdays = weekdays;
  }

  @Override
  public String toString()
  {
    StringBuilder stringBuilder = new StringBuilder();
    stringBuilder.append("Working timespan : ").append(mWorkingTimespan).append("\n")
                 .append("Closed timespans : ");
    for (Timespan timespan : mClosedTimespans)
      stringBuilder.append(timespan).append("\n");
    stringBuilder.append("Fullday : ").append(mIsFullday).append("\n")
                 .append("Weekdays : ");
    for (int i : mWeekdays)
      stringBuilder.append(i);
    return stringBuilder.toString();
  }
}
