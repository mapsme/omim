package com.mapswithme.maps.downloader;

import android.support.annotation.Nullable;

import java.util.List;

public final class DataManager
{
  public interface StorageCallback
  {
    void onStatusChanged(String countryId);
    void onProgress(String countryId, long localSize, long remoteSize);
  }

  private DataManager() {}

  public static native boolean nativeIsLegacyMode();

  public static native @Nullable UpdateInfo nativeGetUpdateInfo();
  public static native void nativeListItems(@Nullable String root, List<CountryItem> result);
  public static native void nativeStartDownload(String countryId);
  public static native void nativeCancelDownload(String countryId);
  public static native int nativeSubscribe(StorageCallback listener);
  public static native void nativeUnsubscribe(int slot);
}
