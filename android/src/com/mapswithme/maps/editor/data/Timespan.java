package com.mapswithme.maps.editor.data;

public class Timespan
{
  public final HoursMinutes mStart;
  public final HoursMinutes mEnd;

  public Timespan(HoursMinutes start, HoursMinutes end)
  {
    mStart = start;
    mEnd = end;
  }

  @Override
  public String toString()
  {
    return "From : " + mStart + " to  : " + mEnd;
  }
}
