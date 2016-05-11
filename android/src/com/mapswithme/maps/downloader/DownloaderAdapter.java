package com.mapswithme.maps.downloader;

import android.app.Activity;
import android.content.DialogInterface;
import android.content.Intent;
import android.graphics.Typeface;
import android.location.Location;
import android.support.annotation.AttrRes;
import android.support.annotation.DrawableRes;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.StringRes;
import android.support.v7.app.AlertDialog;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.style.StyleSpan;
import android.util.SparseArray;
import android.util.SparseIntArray;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Stack;

import com.mapswithme.maps.MwmActivity;
import com.mapswithme.maps.MwmApplication;
import com.mapswithme.maps.R;
import com.mapswithme.maps.background.Notifier;
import com.mapswithme.maps.location.LocationHelper;
import com.mapswithme.maps.routing.RoutingController;
import com.mapswithme.maps.widget.WheelProgressView;
import com.mapswithme.util.BottomSheetHelper;
import com.mapswithme.util.StringUtils;
import com.mapswithme.util.ThemeUtils;
import com.mapswithme.util.UiUtils;
import com.mapswithme.util.statistics.Statistics;
import com.timehop.stickyheadersrecyclerview.StickyRecyclerHeadersAdapter;
import com.timehop.stickyheadersrecyclerview.StickyRecyclerHeadersDecoration;


class DownloaderAdapter extends RecyclerView.Adapter<DownloaderAdapter.ViewHolder>
                     implements StickyRecyclerHeadersAdapter<DownloaderAdapter.HeaderViewHolder>
{
  private final RecyclerView mRecycler;
  private final Activity mActivity;
  private final DownloaderFragment mFragment;
  private final StickyRecyclerHeadersDecoration mHeadersDecoration;

  private boolean mMyMapsMode = true;
  private boolean mSearchResultsMode;
  private String mSearchQuery;

  private final List<CountryItem> mItems = new ArrayList<>();
  private final Map<String, CountryItem> mCountryIndex = new HashMap<>();  // Country.id -> Country

  private final SparseArray<String> mHeaders = new SparseArray<>();
  private final Stack<PathEntry> mPath = new Stack<>();  // Holds navigation history. The last element is the current level.

  private final SparseIntArray mIconsCache = new SparseIntArray();

  private int mListenerSlot;

  private enum MenuItem
  {
    DOWNLOAD(R.drawable.ic_download, R.string.downloader_download_map)
    {
      @Override
      void invoke(final CountryItem item, DownloaderAdapter adapter)
      {
        MapManager.warn3gAndDownload(adapter.mActivity, item.id, new Runnable()
        {
          @Override
          public void run()
          {
            Statistics.INSTANCE.trackEvent(Statistics.EventName.DOWNLOADER_ACTION,
                                           Statistics.params().add(Statistics.EventParam.ACTION, "download")
                                                              .add(Statistics.EventParam.FROM, "downloader")
                                                              .add("is_auto", "false")
                                                              .add("scenario", (item.isExpandable() ? "download_group"
                                                                                                    : "download")));
          }
        });
      }
    },

    DELETE(R.drawable.ic_delete, R.string.delete)
    {
      private void deleteNode(CountryItem item)
      {
        MapManager.nativeCancel(item.id);
        MapManager.nativeDelete(item.id);
        OnmapDownloader.lockAutodownload();

        Statistics.INSTANCE.trackEvent(Statistics.EventName.DOWNLOADER_ACTION,
                                       Statistics.params().add(Statistics.EventParam.ACTION, "delete")
                                                          .add(Statistics.EventParam.FROM, "downloader")
                                                          .add("scenario", (item.isExpandable() ? "delete_group"
                                                                                                : "delete")));
      }

      @Override
      void invoke(final CountryItem item, DownloaderAdapter adapter)
      {
        if (RoutingController.get().isNavigating())
        {
          new AlertDialog.Builder(adapter.mActivity)
              .setTitle(R.string.downloader_delete_map)
              .setMessage(R.string.downloader_delete_map_while_routing_dialog)
              .setPositiveButton(android.R.string.ok, null)
              .show();
          return;
        }

        if (!MapManager.nativeHasUnsavedEditorChanges(item.id))
        {
          deleteNode(item);
          return;
        }

        new AlertDialog.Builder(adapter.mActivity)
                       .setTitle(R.string.downloader_delete_map)
                       .setMessage(R.string.downloader_delete_map_dialog)
                       .setNegativeButton(android.R.string.no, null)
                       .setPositiveButton(android.R.string.yes, new DialogInterface.OnClickListener()
                       {
                         @Override
                         public void onClick(DialogInterface dialog, int which)
                         {
                           deleteNode(item);
                         }
                       }).show();
      }
    },

    CANCEL(R.drawable.ic_cancel, R.string.cancel)
    {
      @Override
      void invoke(CountryItem item, DownloaderAdapter adapter)
      {
        MapManager.nativeCancel(item.id);
        Statistics.INSTANCE.trackEvent(Statistics.EventName.DOWNLOADER_CANCEL,
                                       Statistics.params().add(Statistics.EventParam.FROM, "downloader"));
      }
    },

    EXPLORE(R.drawable.ic_explore, R.string.zoom_to_country)
    {
      @Override
      void invoke(CountryItem item, DownloaderAdapter adapter)
      {
        Intent intent = new Intent(adapter.mActivity, MwmActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
        intent.putExtra(MwmActivity.EXTRA_TASK, new MwmActivity.ShowCountryTask(item.id, false));
        adapter.mActivity.startActivity(intent);

        if (!(adapter.mActivity instanceof MwmActivity))
          adapter.mActivity.finish();

        Statistics.INSTANCE.trackEvent(Statistics.EventName.DOWNLOADER_ACTION,
                                       Statistics.params().add(Statistics.EventParam.ACTION, "explore")
                                                          .add(Statistics.EventParam.FROM, "downloader"));
      }
    },

    UPDATE(R.drawable.ic_update, R.string.downloader_update_map)
    {
      @Override
      void invoke(final CountryItem item, DownloaderAdapter adapter)
      {
        item.update();

        if (item.status != CountryItem.STATUS_UPDATABLE)
          return;

        MapManager.warnOn3gUpdate(adapter.mActivity, item.id, new Runnable()
        {
          @Override
          public void run()
          {
            MapManager.nativeUpdate(item.id);

            Statistics.INSTANCE.trackEvent(Statistics.EventName.DOWNLOADER_ACTION,
                                           Statistics.params().add(Statistics.EventParam.ACTION, "update")
                                                              .add(Statistics.EventParam.FROM, "downloader")
                                                              .add("is_auto", "false")
                                                              .add("scenario", (item.isExpandable() ? "update_group"
                                                                                                    : "update")));
          }
        });
      }
    };

    final @DrawableRes int icon;
    final @StringRes int title;

    MenuItem(@DrawableRes int icon, @StringRes int title)
    {
      this.icon = icon;
      this.title = title;
    }

    abstract void invoke(CountryItem item, DownloaderAdapter adapter);
  }

  private static class PathEntry
  {
    final CountryItem item;
    final boolean myMapsMode;
    final int topPosition;
    final int topOffset;

    private PathEntry(CountryItem item, boolean myMapsMode, int topPosition, int topOffset)
    {
      this.item = item;
      this.myMapsMode = myMapsMode;
      this.topPosition = topPosition;
      this.topOffset = topOffset;
    }

    @Override
    public String toString()
    {
      return item.id + " (" + item.name + "), " +
             "myMapsMode: " + myMapsMode +
             ", topPosition: " + topPosition +
             ", topOffset: " + topOffset;
    }
  }

  private final MapManager.StorageCallback mStorageCallback = new MapManager.StorageCallback()
  {
    private void updateItem(String countryId)
    {
      CountryItem ci = mCountryIndex.get(countryId);
      if (ci == null)
        return;

      ci.update();

      LinearLayoutManager lm = (LinearLayoutManager)mRecycler.getLayoutManager();
      int first = lm.findFirstVisibleItemPosition();
      int last = lm.findLastVisibleItemPosition();
      if (first == RecyclerView.NO_POSITION || last == RecyclerView.NO_POSITION)
        return;

      for (int i = first; i <= last; i++)
      {
        ViewHolder vh = (ViewHolder)mRecycler.findViewHolderForAdapterPosition(i);
        if (vh != null && vh.mItem.id.equals(countryId))
          vh.bind(vh.mItem);
      }
    }

    @Override
    public void onStatusChanged(List<MapManager.StorageCallbackData> data)
    {
      for (MapManager.StorageCallbackData item : data)
        if (item.isLeafNode && item.newStatus == CountryItem.STATUS_FAILED)
        {
          MapManager.showError(mActivity, item, null);
          break;
        }

      if (mSearchResultsMode)
      {
        for (MapManager.StorageCallbackData item : data)
          updateItem(item.countryId);
      }
      else
        refreshData();
    }

    @Override
    public void onProgress(String countryId, long localSize, long remoteSize)
    {
      updateItem(countryId);
    }
  };

  class ViewHolder extends RecyclerView.ViewHolder
  {
    private final WheelProgressView mProgress;
    private final ImageView mStatus;
    private final TextView mName;
    private final TextView mSubtitle;
    private final TextView mFoundName;
    private final TextView mSize;

    private CountryItem mItem;

    private void processClick(boolean clickOnStatus)
    {
      switch (mItem.status)
      {
      case CountryItem.STATUS_DONE:
      case CountryItem.STATUS_PROGRESS:
      case CountryItem.STATUS_ENQUEUED:
        processLongClick();
        break;

      case CountryItem.STATUS_DOWNLOADABLE:
      case CountryItem.STATUS_PARTLY:
        if (clickOnStatus)
          MenuItem.DOWNLOAD.invoke(mItem, DownloaderAdapter.this);
        else
          processLongClick();
        break;

      case CountryItem.STATUS_FAILED:
        MapManager.warn3gAndRetry(mActivity, mItem.id, new Runnable()
        {
          @Override
          public void run()
          {
            Notifier.cancelDownloadFailed();
          }
        });
        break;

      case CountryItem.STATUS_UPDATABLE:
        MapManager.warnOn3gUpdate(mActivity, mItem.id, new Runnable()
        {
          @Override
          public void run()
          {
            MapManager.nativeUpdate(mItem.id);

            Statistics.INSTANCE.trackEvent(Statistics.EventName.DOWNLOADER_ACTION,
                                           Statistics.params().add(Statistics.EventParam.ACTION, "update")
                                                              .add(Statistics.EventParam.FROM, "downloader")
                                                              .add("is_auto", "false")
                                                              .add("scenario", (mItem.isExpandable() ? "update_group"
                                                                                                     : "update")));
          }
        });
        break;

      default:
        throw new IllegalArgumentException("Inappropriate item status: " + mItem.status);
      }
    }

    private void processLongClick()
    {
      List<MenuItem> items = new ArrayList<>();

      switch (mItem.status)
      {
      case CountryItem.STATUS_DOWNLOADABLE:
        items.add(MenuItem.DOWNLOAD);
        break;

      case CountryItem.STATUS_UPDATABLE:
        items.add(MenuItem.UPDATE);
        // No break

      case CountryItem.STATUS_DONE:
        if (!mItem.isExpandable())
          items.add(MenuItem.EXPLORE);

        items.add(MenuItem.DELETE);
        break;

      case CountryItem.STATUS_FAILED:
        items.add(MenuItem.CANCEL);

        if (mItem.present)
        {
          items.add(MenuItem.DELETE);
          items.add(MenuItem.EXPLORE);
        }
        break;

      case CountryItem.STATUS_PROGRESS:
      case CountryItem.STATUS_ENQUEUED:
        items.add(MenuItem.CANCEL);

        if (mItem.present)
          items.add(MenuItem.EXPLORE);
        break;

      case CountryItem.STATUS_PARTLY:
        items.add(MenuItem.DOWNLOAD);
        items.add(MenuItem.DELETE);
        break;
      }

      if (items.isEmpty())
        return;

      BottomSheetHelper.Builder bs = BottomSheetHelper.create(mActivity, mItem.name);
      for (MenuItem item: items)
        bs.sheet(item.ordinal(), item.icon, item.title);

      bs.listener(new android.view.MenuItem.OnMenuItemClickListener()
      {
        @Override
        public boolean onMenuItemClick(android.view.MenuItem item)
        {
          MenuItem.values()[item.getItemId()].invoke(mItem, DownloaderAdapter.this);
          return false;
        }
      }).tint().show();
    }

    public ViewHolder(View frame)
    {
      super(frame);

      mProgress = (WheelProgressView) frame.findViewById(R.id.progress);
      mStatus = (ImageView) frame.findViewById(R.id.status);
      mName = (TextView) frame.findViewById(R.id.name);
      mSubtitle = (TextView) frame.findViewById(R.id.subtitle);
      mFoundName = (TextView) frame.findViewById(R.id.found_name);
      mSize = (TextView) frame.findViewById(R.id.size);

      frame.setOnClickListener(new View.OnClickListener()
      {
        @Override
        public void onClick(View v)
        {
          if (mItem.isExpandable())
            goDeeper(mItem, true);
          else
            processClick(false);
        }
      });

      frame.setOnLongClickListener(new View.OnLongClickListener()
      {
        @Override
        public boolean onLongClick(View v)
        {
          processLongClick();
          return true;
        }
      });

      mStatus.setOnClickListener(new View.OnClickListener()
      {
        @Override
        public void onClick(View v)
        {
          processClick(true);
        }
      });

      mProgress.setOnClickListener(new View.OnClickListener()
      {
        @Override
        public void onClick(View v)
        {
          MapManager.nativeCancel(mItem.id);
          Statistics.INSTANCE.trackEvent(Statistics.EventName.DOWNLOADER_CANCEL,
                                         Statistics.params().add(Statistics.EventParam.FROM, "downloader"));
        }
      });
    }

    private void updateStatus()
    {
      boolean pending = (mItem.status == CountryItem.STATUS_ENQUEUED);
      boolean inProgress = (mItem.status == CountryItem.STATUS_PROGRESS || pending);

      UiUtils.showIf(inProgress, mProgress);
      UiUtils.showIf(!inProgress, mStatus);
      mProgress.setPending(pending);

      if (inProgress)
      {
        if (!pending)
          mProgress.setProgress(mItem.progress);
        return;
      }

      boolean clickable = mItem.isExpandable();
      @AttrRes int iconAttr;

      switch (mItem.status)
      {
      case CountryItem.STATUS_DONE:
        clickable = false;
        iconAttr = R.attr.status_done;
        break;

      case CountryItem.STATUS_DOWNLOADABLE:
      case CountryItem.STATUS_PARTLY:
        iconAttr = (mItem.isExpandable() ? (mMyMapsMode ? R.attr.status_folder_done
                                                        : R.attr.status_folder)
                                         : R.attr.status_downloadable);
        break;

      case CountryItem.STATUS_FAILED:
        iconAttr = R.attr.status_failed;
        break;

      case CountryItem.STATUS_UPDATABLE:
        iconAttr = R.attr.status_updatable;
        break;

      default:
        throw new IllegalArgumentException("Inappropriate item status: " + mItem.status);
      }

      mStatus.setFocusable(clickable);
      mStatus.setImageResource(resolveIcon(iconAttr));
    }

    void bind(CountryItem item)
    {
      mItem = item;

      if (mSearchResultsMode)
      {
        mName.setMaxLines(1);
        mName.setText(mItem.name);

        String found = mItem.searchResultName.toLowerCase();
        SpannableStringBuilder builder = new SpannableStringBuilder(mItem.searchResultName);
        int start = found.indexOf(mSearchQuery);
        int end = start + mSearchQuery.length();

        if (start > -1)
          builder.setSpan(new StyleSpan(Typeface.BOLD), start, end, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);

        mFoundName.setText(builder);

        if (!mItem.isExpandable())
          UiUtils.setTextAndHideIfEmpty(mSubtitle, mItem.topmostParentName);
      }
      else
      {
        mName.setMaxLines(2);
        mName.setText(mItem.name);
        if (!mItem.isExpandable())
          UiUtils.setTextAndHideIfEmpty(mSubtitle, mItem.description);
      }

      if (mItem.isExpandable())
      {
        UiUtils.setTextAndHideIfEmpty(mSubtitle, String.format("%s: %s", mActivity.getString(R.string.downloader_status_maps),
                                                                         mActivity.getString(R.string.downloader_of, mItem.childCount,
                                                                                                                     mItem.totalChildCount)));
      }

      UiUtils.showIf(mSearchResultsMode, mFoundName);
      mSize.setText(StringUtils.getFileSizeString(mItem.totalSize));
      updateStatus();
    }
  }

  class HeaderViewHolder extends RecyclerView.ViewHolder {
    private final TextView mTitle;

    HeaderViewHolder(View frame) {
      super(frame);
      mTitle = (TextView) frame.findViewById(R.id.title);
    }

    void bind(int position) {
      mTitle.setText(mHeaders.get(mItems.get(position).headerId));
    }
  }

  private @DrawableRes int resolveIcon(@AttrRes int iconAttr)
  {
    int res = mIconsCache.get(iconAttr);
    if (res == 0)
    {
      res = ThemeUtils.getResource(mActivity, R.attr.downloaderTheme, iconAttr);
      mIconsCache.put(iconAttr, res);
    }

    return res;
  }

  private void collectHeaders()
  {
    mHeaders.clear();
    if (mSearchResultsMode)
      return;

    int headerId = 0;
    int prev = -1;
    for (CountryItem ci: mItems)
    {
      switch (ci.category)
      {
      case CountryItem.CATEGORY_NEAR_ME:
        if (ci.category != prev)
        {
          headerId = CountryItem.CATEGORY_NEAR_ME;
          mHeaders.put(headerId, MwmApplication.get().getString(R.string.downloader_near_me_subtitle));
          prev = ci.category;
        }
        break;

      case CountryItem.CATEGORY_DOWNLOADED:
        if (ci.category != prev)
        {
          headerId = CountryItem.CATEGORY_DOWNLOADED;
          mHeaders.put(headerId, MwmApplication.get().getString(R.string.downloader_downloaded_subtitle));
          prev = ci.category;
        }
        break;

      default:
        int prevHeader = headerId;
        headerId = CountryItem.CATEGORY_AVAILABLE + ci.name.charAt(0);

        if (headerId != prevHeader)
          mHeaders.put(headerId, ci.name.substring(0, 1).toUpperCase());

        prev = ci.category;
      }

      ci.headerId = headerId;
    }
  }

  void refreshData()
  {
    mSearchResultsMode = false;
    mSearchQuery = null;

    mItems.clear();

    String parent = getCurrentRootId();
    boolean hasLocation = false;
    double lat = 0.0;
    double lon = 0.0;

    if (!mMyMapsMode && CountryItem.isRoot(parent))
    {
      Location loc = LocationHelper.INSTANCE.getSavedLocation();
      hasLocation = (loc != null);
      if (hasLocation)
      {
        lat = loc.getLatitude();
        lon = loc.getLongitude();
      }
    }

    MapManager.nativeListItems(parent, lat, lon, hasLocation, mMyMapsMode, mItems);
    processData();
  }

  void cancelSearch()
  {
    if (mSearchResultsMode)
      refreshData();
  }

  void setSearchResultsMode(Collection<CountryItem> results, String query)
  {
    mSearchResultsMode = true;
    mSearchQuery = query.toLowerCase();

    mItems.clear();
    mItems.addAll(results);
    processData();
  }

  private void processData()
  {
    if (!mSearchResultsMode)
      Collections.sort(mItems);

    collectHeaders();

    mCountryIndex.clear();
    for (CountryItem ci: mItems)
      mCountryIndex.put(ci.id, ci);

    mHeadersDecoration.invalidateHeaders();
    notifyDataSetChanged();

    if (mItems.isEmpty())
      mFragment.setupPlaceholder();

    mFragment.showPlaceholder(mItems.isEmpty());
  }

  DownloaderAdapter(DownloaderFragment fragment)
  {
    mActivity = fragment.getActivity();
    mFragment = fragment;
    mRecycler = mFragment.getRecyclerView();
    mHeadersDecoration = new StickyRecyclerHeadersDecoration(this);
    mRecycler.addItemDecoration(mHeadersDecoration);
  }

  @Override
  public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType)
  {
    return new ViewHolder(LayoutInflater.from(mActivity).inflate(R.layout.downloader_item, parent, false));
  }

  @Override
  public void onBindViewHolder(ViewHolder holder, int position)
  {
    holder.bind(mItems.get(position));
  }

  @Override
  public HeaderViewHolder onCreateHeaderViewHolder(ViewGroup parent)
  {
    return new HeaderViewHolder(LayoutInflater.from(mActivity).inflate(R.layout.downloader_item_header, parent, false));
  }

  @Override
  public void onBindHeaderViewHolder(HeaderViewHolder holder, int position)
  {
    holder.bind(position);
  }

  @Override
  public long getHeaderId(int position)
  {
    return mItems.get(position).headerId;
  }

  @Override
  public int getItemCount()
  {
    return mItems.size();
  }

  private void goDeeper(CountryItem child, boolean refresh)
  {
    LinearLayoutManager lm = (LinearLayoutManager)mRecycler.getLayoutManager();

    // Save scroll positions (top item + item`s offset) for current hierarchy level
    int position = lm.findFirstVisibleItemPosition();
    int offset;

    if (position > -1)
      offset = lm.findViewByPosition(position).getTop();
    else
    {
      position = 0;
      offset = 0;
    }

    boolean wasEmpty = mPath.isEmpty();
    mPath.push(new PathEntry(child, mMyMapsMode, position, offset));
    mMyMapsMode &= (!mSearchResultsMode || child.childCount > 0);

    if (wasEmpty)
      mFragment.clearSearchQuery();

    lm.scrollToPosition(0);

    if (!refresh)
      return;

    refreshData();
    mFragment.update();
  }

  private boolean canGoUpwards()
  {
    return !mPath.isEmpty();
  }

  boolean goUpwards()
  {
    if (!canGoUpwards())
      return false;

    PathEntry entry = mPath.pop();
    mMyMapsMode = entry.myMapsMode;
    refreshData();

    LinearLayoutManager lm = (LinearLayoutManager) mRecycler.getLayoutManager();
    lm.scrollToPositionWithOffset(entry.topPosition, entry.topOffset);

    mFragment.update();
    return true;
  }

  void setAvailableMapsMode()
  {
    goDeeper(getCurrentRootItem(), false);
    mMyMapsMode = false;
    refreshData();
  }

  private CountryItem getCurrentRootItem()
  {
    return (canGoUpwards() ? mPath.peek().item : CountryItem.fill(CountryItem.getRootId()));
  }

  @NonNull String getCurrentRootId()
  {
    return (canGoUpwards() ? getCurrentRootItem().id : CountryItem.getRootId());
  }

  @Nullable String getCurrentRootName()
  {
    return (canGoUpwards() ? getCurrentRootItem().name : null);
  }

  boolean isMyMapsMode()
  {
    return mMyMapsMode;
  }

  void attach()
  {
    mListenerSlot = MapManager.nativeSubscribe(mStorageCallback);
  }

  void detach()
  {
    MapManager.nativeUnsubscribe(mListenerSlot);
  }

  boolean isSearchResultsMode()
  {
    return mSearchResultsMode;
  }
}
