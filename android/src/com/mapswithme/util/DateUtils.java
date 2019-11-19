package com.mapswithme.util;

import androidx.annotation.NonNull;

import java.text.DateFormat;
import java.util.Locale;

public class DateUtils
{
  private DateUtils()
  {
  }

  @NonNull
  public static DateFormat getMediumDateFormatter()
  {
    return DateFormat.getDateInstance(DateFormat.MEDIUM, Locale.getDefault());
  }

  @NonNull
  public static DateFormat getShortDateFormatter()
  {
    return DateFormat.getDateInstance(DateFormat.SHORT, Locale.getDefault());
  }
}
