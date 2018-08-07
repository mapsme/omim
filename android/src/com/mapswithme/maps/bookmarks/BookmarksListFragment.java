package com.mapswithme.maps.bookmarks;

import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.CallSuper;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v7.app.ActionBar;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;

import com.crashlytics.android.Crashlytics;
import com.mapswithme.maps.MwmActivity;
import com.mapswithme.maps.R;
import com.mapswithme.maps.base.BaseMwmRecyclerFragment;
import com.mapswithme.maps.bookmarks.data.Bookmark;
import com.mapswithme.maps.bookmarks.data.BookmarkCategory;
import com.mapswithme.maps.bookmarks.data.BookmarkManager;
import com.mapswithme.maps.bookmarks.data.BookmarkSharingResult;
import com.mapswithme.maps.bookmarks.data.CategoryDataSource;
import com.mapswithme.maps.bookmarks.data.Track;
import com.mapswithme.maps.widget.placepage.EditBookmarkFragment;
import com.mapswithme.maps.widget.placepage.Sponsored;
import com.mapswithme.maps.widget.recycler.ItemDecoratorFactory;
import com.mapswithme.maps.widget.recycler.RecyclerClickListener;
import com.mapswithme.maps.widget.recycler.RecyclerLongClickListener;
import com.mapswithme.util.BottomSheetHelper;
import com.mapswithme.util.UiUtils;
import com.mapswithme.util.sharing.ShareOption;
import com.mapswithme.util.sharing.SharingHelper;

public class BookmarksListFragment extends BaseMwmRecyclerFragment<BookmarkListAdapter>
    implements RecyclerLongClickListener, RecyclerClickListener,
               MenuItem.OnMenuItemClickListener,
               BookmarkManager.BookmarksSharingListener
{
  public static final String TAG = BookmarksListFragment.class.getSimpleName();
  public static final String EXTRA_CATEGORY = "bookmark_category";

  @SuppressWarnings("NullableProblems")
  @NonNull
  private CategoryDataSource mCategoryDataSource;
  private int mSelectedPosition;
  @SuppressWarnings("NullableProblems")
  @NonNull
  private BookmarkManager.BookmarksCatalogListener mCatalogListener;

  @CallSuper
  @Override
  public void onCreate(@Nullable Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);
    Crashlytics.log("onCreate");
    BookmarkCategory category = getCategoryOrThrow();
    mCategoryDataSource = new CategoryDataSource(category);
    mCatalogListener = new CatalogListenerDecorator(this);
  }

  @NonNull
  private BookmarkCategory getCategoryOrThrow()
  {
    Bundle args = getArguments();
    BookmarkCategory category;
    if (args == null || ((category = args.getParcelable(EXTRA_CATEGORY))) == null)
      throw new IllegalArgumentException("Category not exist in bundle");

    return category;
  }

  @NonNull
  @Override
  protected BookmarkListAdapter createAdapter()
  {
    return new BookmarkListAdapter(mCategoryDataSource);
  }

  @Override
  public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState)
  {
    return inflater.inflate(R.layout.fragment_bookmark_list, container, false);
  }

  @CallSuper
  @Override
  public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState)
  {
    super.onViewCreated(view, savedInstanceState);
    Crashlytics.log("onViewCreated");
    configureAdapter();
    setHasOptionsMenu(true);
    boolean isEmpty = getAdapter().getItemCount() == 0;
    UiUtils.showIf(!isEmpty, getRecyclerView());
    showPlaceholder(isEmpty);
    ActionBar bar = ((AppCompatActivity) getActivity()).getSupportActionBar();
    if (bar != null)
      bar.setTitle(mCategoryDataSource.getData().getName());
    addRecyclerDecor();
  }

  private void addRecyclerDecor()
  {
    RecyclerView.ItemDecoration decor = ItemDecoratorFactory
        .createDefaultDecorator(getContext(), LinearLayoutManager.VERTICAL);
    getRecyclerView().addItemDecoration(decor);
  }

  @Override
  public void onStart()
  {
    super.onStart();
    Crashlytics.log("onStart");
    BookmarkManager.INSTANCE.addSharingListener(this);
    BookmarkManager.INSTANCE.addCatalogListener(mCatalogListener);
  }

  @Override
  public void onResume()
  {
    super.onResume();
    Crashlytics.log("onResume");
    BookmarkListAdapter adapter = getAdapter();

    adapter.notifyDataSetChanged();
  }

  @Override
  public void onPause()
  {
    super.onPause();
    Crashlytics.log("onPause");
  }

  @Override
  public void onStop()
  {
    super.onStop();
    Crashlytics.log("onStop");
    BookmarkManager.INSTANCE.removeSharingListener(this);
    BookmarkManager.INSTANCE.removeCatalogListener(mCatalogListener);
  }

  private void configureAdapter()
  {
    BookmarkListAdapter adapter = getAdapter();
    adapter.registerAdapterDataObserver(mCategoryDataSource);
    adapter.setOnClickListener(this);
    adapter.setOnLongClickListener(isCatalogCategory() ? null : this);
  }

  @Override
  public void onItemClick(View v, int position)
  {
    final Intent i = new Intent(getActivity(), MwmActivity.class);

    BookmarkListAdapter adapter = getAdapter();

    switch (adapter.getItemViewType(position))
    {
      case BookmarkListAdapter.TYPE_SECTION:
      case BookmarkListAdapter.TYPE_DESC:
        return;
      case BookmarkListAdapter.TYPE_BOOKMARK:
        final Bookmark bookmark = (Bookmark) adapter.getItem(position);
        i.putExtra(MwmActivity.EXTRA_TASK,
                   new MwmActivity.ShowBookmarkTask(bookmark.getCategoryId(), bookmark.getBookmarkId()));
        break;
      case BookmarkListAdapter.TYPE_TRACK:
        final Track track = (Track) adapter.getItem(position);
        i.putExtra(MwmActivity.EXTRA_TASK,
                   new MwmActivity.ShowTrackTask(track.getCategoryId(), track.getTrackId()));
        break;
    }

    i.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
    startActivity(i);
  }

  @Override
  public void onLongItemClick(View v, int position)
  {
    BookmarkListAdapter adapter = getAdapter();

    mSelectedPosition = position;
    int type = adapter.getItemViewType(mSelectedPosition);

    switch (type)
    {
      case BookmarkListAdapter.TYPE_SECTION:
      case BookmarkListAdapter.TYPE_DESC:
        // Do nothing here?
        break;

      case BookmarkListAdapter.TYPE_BOOKMARK:
        final Bookmark bookmark = (Bookmark) adapter.getItem(mSelectedPosition);
        int menuResId = isCatalogCategory() ? R.menu.menu_bookmarks_catalog : R.menu.menu_bookmarks;
        BottomSheetHelper.Builder bs = BottomSheetHelper.create(getActivity(), bookmark.getTitle())
                                                        .sheet(menuResId)
                                                        .listener(this);
        bs.tint().show();
        break;

      case BookmarkListAdapter.TYPE_TRACK:
        final Track track = (Track) adapter.getItem(mSelectedPosition);
        BottomSheetHelper.create(getActivity(), track.getName())
                         .sheet(Menu.NONE, R.drawable.ic_delete, R.string.delete)
                         .listener(new MenuItem.OnMenuItemClickListener()
                         {
                           @Override
                           public boolean onMenuItemClick(MenuItem menuItem)
                           {
                             BookmarkManager.INSTANCE.deleteTrack(track.getTrackId());
                             adapter.notifyDataSetChanged();
                             return false;
                           }
                         }).tint().show();
        break;
    }
  }

  @Override
  public void onPreparedFileForSharing(@NonNull BookmarkSharingResult result)
  {
    SharingHelper.INSTANCE.onPreparedFileForSharing(getActivity(), result);
  }

  @Override
  public boolean onMenuItemClick(MenuItem menuItem)
  {
    BookmarkListAdapter adapter = getAdapter();

    Bookmark item = (Bookmark) adapter.getItem(mSelectedPosition);

    switch (menuItem.getItemId())
    {
    case R.id.share:
      ShareOption.ANY.shareMapObject(getActivity(), item, Sponsored.nativeGetCurrent());
      break;

    case R.id.edit:
      EditBookmarkFragment.editBookmark(item.getCategoryId(), item.getBookmarkId(), getActivity(),
                                        getChildFragmentManager(), new EditBookmarkFragment.EditBookmarkListener()
          {
            @Override
            public void onBookmarkSaved(long bookmarkId)
            {
              adapter.notifyDataSetChanged();
            }
          });
      break;

    case R.id.delete:
      BookmarkManager.INSTANCE.deleteBookmark(item.getBookmarkId());
      adapter.notifyDataSetChanged();
      break;
    }
    return false;
  }

  @Override
  public void onCreateOptionsMenu(Menu menu, MenuInflater inflater)
  {
    if (isCatalogCategory())
      return;
    inflater.inflate(R.menu.option_menu_bookmarks, menu);
  }

  private boolean isCatalogCategory()
  {
    return mCategoryDataSource.getData().getType() == BookmarkCategory.Type.CATALOG;
  }

  @Override
  public boolean onOptionsItemSelected(MenuItem item)
  {
    if (item.getItemId() == R.id.set_share)
    {
      SharingHelper.INSTANCE.prepareBookmarkCategoryForSharing(getActivity(),
                                                               mCategoryDataSource.getData().getId());
      return true;
    }

    return super.onOptionsItemSelected(item);
  }
}
