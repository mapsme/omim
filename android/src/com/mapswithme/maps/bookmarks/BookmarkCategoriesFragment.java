package com.mapswithme.maps.bookmarks;

import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.CallSuper;
import android.support.annotation.LayoutRes;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v7.widget.RecyclerView;
import android.view.MenuItem;
import android.view.View;
import android.widget.Toast;

import com.mapswithme.maps.R;
import com.mapswithme.maps.auth.Authorizer;
import com.mapswithme.maps.base.BaseMwmRecyclerFragment;
import com.mapswithme.maps.bookmarks.data.BookmarkBackupController;
import com.mapswithme.maps.bookmarks.data.BookmarkManager;
import com.mapswithme.maps.dialog.EditTextDialogFragment;
import com.mapswithme.maps.widget.PlaceholderView;
import com.mapswithme.maps.widget.recycler.ItemDecoratorFactory;
import com.mapswithme.maps.widget.recycler.RecyclerClickListener;
import com.mapswithme.maps.widget.recycler.RecyclerLongClickListener;
import com.mapswithme.util.BottomSheetHelper;
import com.mapswithme.util.UiUtils;
import com.mapswithme.util.sharing.SharingHelper;

public class BookmarkCategoriesFragment extends BaseMwmRecyclerFragment
    implements EditTextDialogFragment.OnTextSaveListener,
               MenuItem.OnMenuItemClickListener,
               RecyclerClickListener,
               RecyclerLongClickListener,
               BookmarkManager.BookmarksLoadingListener,
               BookmarkCategoriesAdapter.CategoryListInterface
{
  private long mSelectedCatId;
  @Nullable
  private View mLoadingPlaceholder;

  @Nullable
  private BookmarkBackupController mBackupController;

  @Override
  protected @LayoutRes int getLayoutRes()
  {
    return R.layout.fragment_bookmark_categories;
  }

  @Override
  protected RecyclerView.Adapter createAdapter()
  {
    return new BookmarkCategoriesAdapter(getActivity());
  }

  @Nullable
  @Override
  protected BookmarkCategoriesAdapter getAdapter()
  {
    RecyclerView.Adapter adapter = super.getAdapter();
    return adapter != null ? (BookmarkCategoriesAdapter) adapter : null;
  }

  @CallSuper
  @Override
  public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState)
  {
    super.onViewCreated(view, savedInstanceState);

    mLoadingPlaceholder = view.findViewById(R.id.placeholder_loading);
    mBackupController = new BookmarkBackupController(view.findViewById(R.id.backup),
                                                     new Authorizer(this));
    if (getAdapter() != null)
    {
      getAdapter().setOnClickListener(this);
      getAdapter().setOnLongClickListener(this);
      getAdapter().setCategoryListInterface(this);
      getAdapter().registerAdapterDataObserver(new RecyclerView.AdapterDataObserver()
      {
        @Override
        public void onChanged()
        {
          updateResultsPlaceholder();
        }
      });
    }

    getRecyclerView().setNestedScrollingEnabled(false);
    getRecyclerView().addItemDecoration(ItemDecoratorFactory.createVerticalDefaultDecorator(getContext()));
  }

  private void updateResultsPlaceholder()
  {
    boolean showLoadingPlaceholder = BookmarkManager.isAsyncBookmarksLoadingInProgress();
    boolean showPlaceHolder = !showLoadingPlaceholder &&
                              (getAdapter() == null || getAdapter().getItemCount() == 0);
    if (getAdapter() != null)
      showPlaceholder(showPlaceHolder);

    View root = getView();
    if (root != null)
      UiUtils.showIf(!showLoadingPlaceholder && !showPlaceHolder, root, R.id.backup);
  }

  private void updateLoadingPlaceholder()
  {
    if (mLoadingPlaceholder != null)
    {
      boolean showLoadingPlaceholder = BookmarkManager.isAsyncBookmarksLoadingInProgress();
      if (getAdapter() != null && getAdapter().getItemCount() != 0)
        showLoadingPlaceholder = false;

      UiUtils.showIf(showLoadingPlaceholder, mLoadingPlaceholder);
    }
  }

  @Override
  public void onStart()
  {
    super.onStart();
    BookmarkManager.INSTANCE.addListener(this);
    if (mBackupController != null)
      mBackupController.onStart();
  }

  @Override
  public void onStop()
  {
    super.onStop();
    BookmarkManager.INSTANCE.removeListener(this);
    if (mBackupController != null)
      mBackupController.onStop();
  }

  @Override
  public void onResume()
  {
    super.onResume();
    updateLoadingPlaceholder();
    if (getAdapter() != null)
      getAdapter().notifyDataSetChanged();
  }

  @Override
  public void onPause()
  {
    super.onPause();
    BottomSheetHelper.free();
  }

  @Override
  public void onSaveText(String text)
  {
    BookmarkManager.INSTANCE.setCategoryName(mSelectedCatId, text);
    if (getAdapter() != null)
      getAdapter().notifyDataSetChanged();
  }

  @Override
  public boolean onMenuItemClick(MenuItem item)
  {
    switch (item.getItemId())
    {
    case R.id.set_show:
      BookmarkManager.INSTANCE.toggleCategoryVisibility(mSelectedCatId);
      if (getAdapter() != null)
        getAdapter().notifyDataSetChanged();
      break;

    case R.id.set_share:
      SharingHelper.shareBookmarksCategory(getActivity(), mSelectedCatId);
      break;

    case R.id.set_delete:
      BookmarkManager.INSTANCE.deleteCategory(mSelectedCatId);
      if (getAdapter() != null)
        getAdapter().notifyDataSetChanged();
      break;

    case R.id.set_edit:
      EditTextDialogFragment.show(getString(R.string.bookmark_set_name),
                                  BookmarkManager.INSTANCE.getCategoryName(mSelectedCatId),
                                  getString(R.string.rename), getString(R.string.cancel), this);
      break;
    }

    return true;
  }

  @Override
  public void onLongItemClick(View v, int position)
  {
    showBottomMenu(position);
  }

  private void showBottomMenu(int position)
  {
    final BookmarkManager bmManager = BookmarkManager.INSTANCE;
    mSelectedCatId = bmManager.getCategoryIdByPosition(position);

    final String name = bmManager.getCategoryName(mSelectedCatId);
    BottomSheetHelper.Builder bs = BottomSheetHelper.create(getActivity(), name)
                                                    .sheet(R.menu.menu_bookmark_categories)
                                                    .listener(this);
    MenuItem show = bs.getMenu().getItem(0);
    final boolean isVisible = bmManager.isVisible(mSelectedCatId);
    show.setIcon(isVisible ? R.drawable.ic_hide
                           : R.drawable.ic_show);
    show.setTitle(isVisible ? R.string.hide
                            : R.string.show);
    bs.tint().show();
  }

  @Override
  public void onMoreOperationClick(int position)
  {
    showBottomMenu(position);
  }

  @Override
  public void onItemClick(View v, int position)
  {
    startActivity(new Intent(getActivity(), BookmarkListActivity.class)
                      .putExtra(ChooseBookmarkCategoryFragment.CATEGORY_POSITION, position));
  }

  @Override
  protected void setupPlaceholder(@NonNull PlaceholderView placeholder)
  {
    placeholder.setContent(R.drawable.img_bookmarks, R.string.bookmarks_empty_title,
                           R.string.bookmarks_usage_hint);
  }

  @Override
  public void onBookmarksLoadingStarted()
  {
    updateLoadingPlaceholder();
    updateResultsPlaceholder();
  }

  @Override
  public void onBookmarksLoadingFinished()
  {
    updateLoadingPlaceholder();
    updateResultsPlaceholder();
    if (getAdapter() != null)
      getAdapter().notifyDataSetChanged();
  }

  @Override
  public void onBookmarksFileLoaded(boolean success)
  {
    // Do nothing here.
  }

  @Override
  public void onAddCategory()
  {
    Toast.makeText(getContext(), "Coming soon", Toast.LENGTH_LONG).show();
  }

  @Override
  public void onActivityResult(int requestCode, int resultCode, Intent data)
  {
    super.onActivityResult(requestCode, resultCode, data);
    if (mBackupController != null)
      mBackupController.onActivityResult(requestCode, resultCode, data);
  }
}
