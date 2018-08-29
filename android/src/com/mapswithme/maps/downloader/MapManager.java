package com.mapswithme.maps.downloader;

import android.app.Activity;
import android.app.Application;
import android.content.DialogInterface;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.StringRes;
import android.support.annotation.UiThread;
import android.support.v7.app.AlertDialog;
import android.text.TextUtils;

import com.mapswithme.maps.R;
import com.mapswithme.util.ConnectionState;
import com.mapswithme.util.Utils;
import com.mapswithme.util.statistics.Statistics;

import java.lang.ref.WeakReference;
import java.util.List;

@UiThread
public final class MapManager
{
  @SuppressWarnings("unused")
  public static class StorageCallbackData
  {
    public final String countryId;
    public final int newStatus;
    public final int errorCode;
    public final boolean isLeafNode;

    public StorageCallbackData(String countryId, int newStatus, int errorCode, boolean isLeafNode)
    {
      this.countryId = countryId;
      this.newStatus = newStatus;
      this.errorCode = errorCode;
      this.isLeafNode = isLeafNode;
    }
  }

  @SuppressWarnings("unused")
  public interface StorageCallback
  {
    void onStatusChanged(List<StorageCallbackData> data);
    void onProgress(String countryId, long localSize, long remoteSize);
  }

  @SuppressWarnings("unused")
  interface CurrentCountryChangedListener
  {
    void onCurrentCountryChanged(String countryId);
  }

  @SuppressWarnings("unused")
  interface MigrationListener
  {
    void onComplete();
    void onProgress(int percent);
    void onError(int code);
  }

  private static WeakReference<AlertDialog> sCurrentErrorDialog;

  private MapManager() {}

  public static void sendErrorStat(String event, int code)
  {
    String text;
    switch (code)
    {
    case CountryItem.ERROR_NO_INTERNET:
      text = "no_connection";
      break;

    case CountryItem.ERROR_OOM:
      text = "no_space";
      break;

    default:
      text = "unknown_error";
    }

    Statistics.INSTANCE.trackEvent(event, Statistics.params().add(Statistics.EventParam.TYPE, text));
  }

  public static void showError(final Activity activity, final StorageCallbackData errorData,
                               @Nullable final Utils.Proc<Boolean> dialogClickListener)
  {
    if (!nativeIsAutoretryFailed())
      return;

    showErrorDialog(activity, errorData, dialogClickListener);
  }

  public static void showErrorDialog(final Activity activity, final StorageCallbackData errorData,
                                     @Nullable final Utils.Proc<Boolean> dialogClickListener)
  {
    if (sCurrentErrorDialog != null)
    {
      AlertDialog dlg = sCurrentErrorDialog.get();
      if (dlg != null && dlg.isShowing())
        return;
    }

    @StringRes int text;
    switch (errorData.errorCode)
    {
    case CountryItem.ERROR_NO_INTERNET:
      text = R.string.common_check_internet_connection_dialog;
      break;

    case CountryItem.ERROR_OOM:
      text = R.string.downloader_no_space_title;
      break;

    default:
      throw new IllegalArgumentException("Given error can not be displayed: " + errorData.errorCode);
    }

    AlertDialog dlg = new AlertDialog.Builder(activity)
                                     .setTitle(R.string.country_status_download_failed)
                                     .setMessage(text)
                                     .setNegativeButton(android.R.string.cancel, new DialogInterface.OnClickListener()
                                     {
                                       @Override
                                       public void onClick(DialogInterface dialog, int which)
                                       {
                                         sCurrentErrorDialog = null;
                                         if (dialogClickListener != null)
                                           dialogClickListener.invoke(false);
                                       }
                                     })
                                     .setPositiveButton(R.string.downloader_retry, new DialogInterface.OnClickListener()
                                     {
                                       @Override
                                       public void onClick(DialogInterface dialog, int which)
                                       {
                                         Application app = activity.getApplication();
                                         RetryFailedDownloadConfirmationListener listener
                                             = new ExpandRetryConfirmationListener(app, dialogClickListener);
                                         warn3gAndRetry(activity, errorData.countryId, listener);
                                       }
                                     }).create();
    dlg.setCanceledOnTouchOutside(false);
    dlg.show();
    sCurrentErrorDialog = new WeakReference<>(dlg);
  }

  private static void notifyNoSpaceInternal(Activity activity)
  {
    new AlertDialog.Builder(activity)
        .setTitle(R.string.downloader_no_space_title)
        .setMessage(R.string.downloader_no_space_message)
        .setPositiveButton(android.R.string.ok, null)
        .show();
  }

  /**
   * @return true if there is no space to update the given {@code root}, so the alert dialog will be shown.
   */
  private static boolean notifyNoSpaceToUpdate(Activity activity, String root)
  {
    if (nativeHasSpaceToUpdate(root))
      return false;

    notifyNoSpaceInternal(activity);
    return true;
  }

  /**
   * @return true if there is no space to download the given {@code root}, so the alert dialog will be shown.
   */
  private static boolean notifyNoSpace(Activity activity, String root)
  {
    if (nativeHasSpaceToDownloadCountry(root))
      return false;

    notifyNoSpaceInternal(activity);
    return true;
  }

  /**
   * @return true if there is no space to download {@code size} bytes, so the alert dialog will be shown.
   */
  private static boolean notifyNoSpace(Activity activity, long size)
  {
    if (nativeHasSpaceToDownloadAmount(size))
      return false;

    notifyNoSpaceInternal(activity);
    return true;
  }

  private static boolean warnOn3gInternal(Activity activity, @NonNull final Runnable onAcceptListener)
  {
    if (nativeIsDownloadOn3gEnabled() || !ConnectionState.isMobileConnected())
    {
      onAcceptListener.run();
      return false;
    }

    new AlertDialog.Builder(activity)
        .setMessage(String.format("%1$s\n\n%2$s", activity.getString(R.string.download_over_mobile_header),
                                                  activity.getString(R.string.download_over_mobile_message)))
        .setNegativeButton(android.R.string.no, null)
        .setPositiveButton(android.R.string.yes, new DialogInterface.OnClickListener()
        {
          @Override
          public void onClick(DialogInterface dlg, int which)
          {
            nativeEnableDownloadOn3g();
            onAcceptListener.run();
          }
        }).show();

    return true;
  }

  static boolean warnOn3gUpdate(Activity activity, @Nullable String countryId, @NonNull final Runnable onAcceptListener)
  {
    //noinspection SimplifiableIfStatement
    if (TextUtils.isEmpty(countryId) || !notifyNoSpaceToUpdate(activity, countryId))
      return warnOn3gInternal(activity, onAcceptListener);

    return true;
  }

  static boolean warnOn3g(Activity activity, @Nullable String countryId, @NonNull final Runnable onAcceptListener)
  {
    //noinspection SimplifiableIfStatement
    if (TextUtils.isEmpty(countryId) || !notifyNoSpace(activity, countryId))
      return warnOn3gInternal(activity, onAcceptListener);

    return true;
  }

  public static boolean warnOn3g(Activity activity, long size, @NonNull Runnable onAcceptListener)
  {
    return !notifyNoSpace(activity, size) && warnOn3gInternal(activity, onAcceptListener);
  }

  public static boolean warn3gAndDownload(Activity activity, final String countryId, @Nullable final Runnable onAcceptListener)
  {
    return warnOn3g(activity, countryId, new Runnable()
    {
      @Override
      public void run()
      {
        if (onAcceptListener != null)
          onAcceptListener.run();
        nativeDownload(countryId);
      }
    });
  }

  static boolean warn3gAndRetry(Activity activity, final String countryId, @Nullable final Runnable onAcceptListener)
  {
    return warnOn3g(activity, countryId, new Runnable()
    {
      @Override
      public void run()
      {
        if (onAcceptListener != null)
          onAcceptListener.run();
        nativeRetry(countryId);
      }
    });
  }

  /**
   * Retrieves ID of root node.
   */
  public static native String nativeGetRoot();

  /**
   * Moves a file from one place to another.
   */
  public static native boolean nativeMoveFile(String oldFile, String newFile);

  /**
   * Returns {@code true} if there is enough storage space to perform migration. Or {@code false} otherwise.
   */
  public static native boolean nativeHasSpaceForMigration();

  /**
   * Returns {@code true} if there is enough storage space to download specified amount of data. Or {@code false} otherwise.
   */
  public static native boolean nativeHasSpaceToDownloadAmount(long bytes);

  /**
   * Returns {@code true} if there is enough storage space to download maps with specified {@code root}. Or {@code false} otherwise.
   */
  public static native boolean nativeHasSpaceToDownloadCountry(String root);

  /**
   * Returns {@code true} if there is enough storage space to update maps with specified {@code root}. Or {@code false} otherwise.
   */
  public static native boolean nativeHasSpaceToUpdate(String root);

  /**
   * Determines whether the legacy (large MWMs) mode is used.
   */
  public static native boolean nativeIsLegacyMode();

  /**
   * Quickly determines if the migration is needed. In the most cases you should use {@link #nativeIsLegacyMode()} instead.
   */
  public static native boolean nativeNeedMigrate();

  /**
   * Performs migration from old (large MWMs) mode.
   * @return Name of the country to be loaded during the prefetch.
   *         Or {@code null} if maps were queued to downloader and migration process is complete.
   *         In the latter case {@link MigrationListener#onComplete()} will be called before return from {@code nativeMigrate()}.
   */
  public static native @Nullable String nativeMigrate(MigrationListener listener, double lat, double lon, boolean hasLocation, boolean keepOldMaps);

  /**
   * Aborts migration. Affects only prefetch process.
   */
  public static native void nativeCancelMigration();

  /**
   * Return count of fully downloaded maps (excluding fake MWMs).
   */
  public static native int nativeGetDownloadedCount();

  /**
   * Returns info about updatable data under given {@code root} or null on error.
   */
  public static native @Nullable UpdateInfo nativeGetUpdateInfo(@Nullable String root);

  /**
   * Retrieves list of country items with its status info.
   * if {@code root} is {@code null}, list of downloaded countries is returned.
   */
  public static native void nativeListItems(@Nullable String root, double lat, double lon, boolean hasLocation, boolean myMapsMode, List<CountryItem> result);

  /**
   * Sets following attributes of the given {@code item}:
   * <pre>
   * <ul>
   *   <li>name;</li>
   *   <li>directParentId;</li>
   *   <li>topmostParentId;</li>
   *   <li>directParentName;</li>
   *   <li>topmostParentName;</li>
   *   <li>description;</li>
   *   <li>size;</li>
   *   <li>enqueuedSize;</li>
   *   <li>totalSize;</li>
   *   <li>childCount;</li>
   *   <li>totalChildCount;</li>
   *   <li>status;</li>
   *   <li>errorCode;</li>
   *   <li>present;</li>
   *   <li>progress</li>
   * </ul>
   * </pre>
   */
  public static native void nativeGetAttributes(CountryItem item);

  /**
   * Returns status for given {@code root} node.
   */
  public static native int nativeGetStatus(String root);

  /**
   * Returns downloading error for given {@code root} node.
   */
  public static native int nativeGetError(String root);

  /**
   * Returns localized name for given {@code root} node.
   */
  public static native String nativeGetName(String root);

  /**
   * Returns country ID corresponding to given coordinates or {@code null} on error.
   */
  public static native @Nullable String nativeFindCountry(double lat, double lon);

  /**
   * Determines whether something is downloading now.
   */
  public static native boolean nativeIsDownloading();

  @Nullable
  public static native String nativeGetCurrentDownloadingCountryId();

  /**
   * Enqueues given {@code root} node and its children in downloader.
   */
  public static native void nativeDownload(String root);

  /**
   * Enqueues failed items under given {@code root} node in downloader.
   */
  public static native void nativeRetry(String root);

  /**
   * Enqueues given {@code root} node with its children in downloader.
   */
  public static native void nativeUpdate(String root);

  /**
   * Removes given currently downloading {@code root} node and its children from downloader.
   */
  public static native void nativeCancel(String root);

  /**
   * Deletes given installed {@code root} node with its children.
   */
  public static native void nativeDelete(String root);

  /**
   * Registers {@code callback} of storage status changed. Returns slot ID which should be used to unsubscribe in {@link #nativeUnsubscribe(int)}.
   */
  public static native int nativeSubscribe(StorageCallback callback);

  /**
   * Unregisters storage status changed callback.
   * @param slot Slot ID returned from {@link #nativeSubscribe(StorageCallback)} while registering.
   */
  public static native void nativeUnsubscribe(int slot);

  /**
   * Sets callback about current country change. Single subscriber only.
   */
  public static native void nativeSubscribeOnCountryChanged(CurrentCountryChangedListener listener);

  /**
   * Removes callback about current country change.
   */
  public static native void nativeUnsubscribeOnCountryChanged();

  /**
   * Determines if there are unsaved editor changes present for given {@code root}.
   */
  public static native boolean nativeHasUnsavedEditorChanges(String root);

  /**
   * Fills given {@code result} list with intermediate nodes from the root node (including) to the given {@code root} (excluding).
   * For instance, for {@code root == "Florida"} the resulting list is filled with values: {@code { "United States of America", "Countries" }}.
   */
  public static native void nativeGetPathTo(String root, List<String> result);

  /**
   * Calculates joint progress of downloading countries specified by {@code countries} array.
   * @return 0 to 100 percent.
   */
  public static native int nativeGetOverallProgress(String[] countries);

  /**
   * Returns {@code true} if the core will NOT do attempts to download failed maps anymore.
   */
  public static native boolean nativeIsAutoretryFailed();

  /**
   * Returns {@code true} if the core is allowed to download maps while on 3g network. {@code false} otherwise.
   */
  public static native boolean nativeIsDownloadOn3gEnabled();

  /**
   * Sets flag which allows to download maps on 3G.
   */
  public static native void nativeEnableDownloadOn3g();

  /**
   * Returns country ID which the current PP object points to, or {@code null}.
   */
  public static native @Nullable String nativeGetSelectedCountry();
}
