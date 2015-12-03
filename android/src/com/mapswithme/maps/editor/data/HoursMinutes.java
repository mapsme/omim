package com.mapswithme.maps.editor.data;

import android.os.Parcel;
import android.os.Parcelable;
import android.support.annotation.IntRange;

public class HoursMinutes implements Parcelable
{
  public final long mHours;
  public final long mMinutes;

  public HoursMinutes(@IntRange(from = 0, to = 24) long hours, @IntRange(from = 0, to = 60) long minutes)
  {
    mHours = hours;
    mMinutes = minutes;
  }

  protected HoursMinutes(Parcel in)
  {
    mHours = in.readLong();
    mMinutes = in.readLong();
  }

  @Override
  public String toString()
  {
    return mHours + "." + mMinutes;
  }

  @Override
  public int describeContents()
  {
    return 0;
  }

  @Override
  public void writeToParcel(Parcel dest, int flags)
  {
    dest.writeLong(mHours);
    dest.writeLong(mMinutes);
  }

  public static final Creator<HoursMinutes> CREATOR = new Creator<HoursMinutes>()
  {
    @Override
    public HoursMinutes createFromParcel(Parcel in)
    {
      return new HoursMinutes(in);
    }

    @Override
    public HoursMinutes[] newArray(int size)
    {
      return new HoursMinutes[size];
    }
  };
}
