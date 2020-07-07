package com.mapswithme.maps.search;

import android.os.Parcel;
import android.os.Parcelable;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.mapswithme.util.ConnectionState;

public class BookingFilterParams implements Parcelable
{
  protected BookingFilterParams(Parcel in)
  {
    mCheckinMillisec = in.readLong();
    mCheckoutMillisec = in.readLong();
    mRooms = in.createTypedArray(Room.CREATOR);
  }

  public static final Creator<BookingFilterParams> CREATOR = new Creator<BookingFilterParams>()
  {
    @Override
    public BookingFilterParams createFromParcel(Parcel in)
    {
      return new BookingFilterParams(in);
    }

    @Override
    public BookingFilterParams[] newArray(int size)
    {
      return new BookingFilterParams[size];
    }
  };

  @Override
  public int describeContents()
  {
    return 0;
  }

  @Override
  public void writeToParcel(Parcel dest, int flags)
  {
    dest.writeLong(mCheckinMillisec);
    dest.writeLong(mCheckoutMillisec);
    dest.writeTypedArray(mRooms, flags);
  }

  static class Room implements Parcelable
  {
    static final Room DEFAULT = new Room(2, null);
    private int mAdultsCount;
    @Nullable
    private int[] mAgeOfChildren;

    Room(int adultsCount, @Nullable int[] ageOfChildren)
    {
      mAdultsCount = adultsCount;
      mAgeOfChildren = ageOfChildren;
    }

    protected Room(Parcel in)
    {
      mAdultsCount = in.readInt();
      mAgeOfChildren = in.createIntArray();
    }

    public static final Creator<Room> CREATOR = new Creator<Room>()
    {
      @Override
      public Room createFromParcel(Parcel in)
      {
        return new Room(in);
      }

      @Override
      public Room[] newArray(int size)
      {
        return new Room[size];
      }
    };

    @Override
    public int describeContents()
    {
      return 0;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags)
    {
      dest.writeInt(mAdultsCount);
      dest.writeIntArray(mAgeOfChildren);
    }
  }

  private long mCheckinMillisec;
  private long  mCheckoutMillisec;
  @NonNull
  private final Room[] mRooms;

  private BookingFilterParams(long checkinMillisec, long checkoutMillisec, @NonNull Room... rooms)
  {
    mCheckinMillisec = checkinMillisec;
    mCheckoutMillisec = checkoutMillisec;
    mRooms = rooms;
  }

  public long getCheckinMillisec()
  {
    return mCheckinMillisec;
  }

  public long getCheckoutMillisec()
  {
    return mCheckoutMillisec;
  }

  @NonNull
  public Room[] getRooms()
  {
    return mRooms;
  }


  public static class Factory
  {
    @Nullable
    public BookingFilterParams createParams(long checkIn, long checkOut, @NonNull Room... rooms)
    {
      return ConnectionState.isConnected() ? new BookingFilterParams(checkIn, checkOut, rooms)
                                           : null;
    }
  }
}
