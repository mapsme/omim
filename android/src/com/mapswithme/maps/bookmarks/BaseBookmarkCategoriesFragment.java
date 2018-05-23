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

import com.mapswithme.maps.R;
import com.mapswithme.maps.base.BaseMwmRecyclerFragment;
import com.mapswithme.maps.bookmarks.data.BookmarkCategory;
import com.mapswithme.maps.bookmarks.data.BookmarkManager;
import com.mapswithme.maps.bookmarks.data.BookmarkSharingResult;
import com.mapswithme.maps.dialog.EditTextDialogFragment;
import com.mapswithme.maps.widget.PlaceholderView;
import com.mapswithme.maps.widget.recycler.ItemDecoratorFactory;
import com.mapswithme.util.BottomSheetHelper;
import com.mapswithme.util.UiUtils;
import com.mapswithme.util.sharing.SharingHelper;

public abstract class BaseBookmarkCategoriesFragment extends BaseMwmRecyclerFragment
    implements EditTextDialogFragment.EditTextDialogInterface,
               MenuItem.OnMenuItemClickListener,
               BookmarkManager.BookmarksLoadingListener,
               BookmarkManager.BookmarksSharingListener,
               CategoryListCallback,
               KmlImportController.ImportKmlCallback,
               OnItemClickListener<BookmarkCategory>,
               OnItemLongClickListener<BookmarkCategory>


{
  private static final int MAX_CATEGORY_NAME_LENGTH = 60;
  @Nullable
  private BookmarkCategory mSelectedCategory;
  @Nullable
  private CategoryEditor mCategoryEditor;
  @Nullable
  private KmlImportController mKmlImportController;
  @NonNull
  private Runnable mImportKmlTask = new ImportKmlTask();

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

    onPrepareControllers(view);
    if (getAdapter() != null)
    {
      getAdapter().setOnClickListener(this);
      getAdapter().setOnLongClickListener(this);
      getAdapter().setCategoryListCallback(this);
    }

    RecyclerView rw = getRecyclerView();
    if (rw == null)
      return;

    rw.setNestedScrollingEnabled(false);
    rw.addItemDecoration(ItemDecoratorFactory.createVerticalDefaultDecorator(getContext()));
  }

  protected void onPrepareControllers(@NonNull View view)
  {
    mKmlImportController = new KmlImportController(getActivity(), this);
  }

  protected void updateLoadingPlaceholder()
  {
    View root = getView();
    if (root == null)
      throw new AssertionError("Fragment view must be non-null at this point!");

    View loadingPlaceholder = root.findViewById(R.id.placeholder_loading);
    boolean showLoadingPlaceholder = BookmarkManager.INSTANCE.isAsyncBookmarksLoadingInProgress();
    UiUtils.showIf(showLoadingPlaceholder, loadingPlaceholder);
  }

  @Override
  public void onStart()
  {
    super.onStart();
    BookmarkManager.INSTANCE.addLoadingListener(this);
    BookmarkManager.INSTANCE.addSharingListener(this);
    if (mKmlImportController != null)
    {
      mKmlImportController.onStart();
    }
  }

  @Override
  public void onStop()
  {
    super.onStop();
    BookmarkManager.INSTANCE.removeLoadingListener(this);
    BookmarkManager.INSTANCE.removeSharingListener(this);
    if (mKmlImportController != null)
      mKmlImportController.onStop();
  }

  @Override
  public void onResume()
  {
    super.onResume();
    updateLoadingPlaceholder();
    if (getAdapter() != null)
      getAdapter().notifyDataSetChanged();
    if (!BookmarkManager.INSTANCE.isAsyncBookmarksLoadingInProgress())
      mImportKmlTask.run();
  }

  @Override
  public void onPause()
  {
    super.onPause();
  }

  @Override
  public boolean onMenuItemClick(MenuItem item)
  {
    switch (item.getItemId())
    {
    case R.id.set_show:
      BookmarkManager.INSTANCE.toggleCategoryVisibility(mSelectedCategory.getId());
      if (getAdapter() != null)
        getAdapter().notifyDataSetChanged();
      break;

    case R.id.set_share:
      SharingHelper.INSTANCE.prepareBookmarkCategoryForSharing(getActivity(), mSelectedCategory.getId());
      break;

    case R.id.set_delete:
      BookmarkManager.INSTANCE.deleteCategory(mSelectedCategory.getId());
      if (getAdapter() != null)
        getAdapter().notifyDataSetChanged();
      break;

    case R.id.set_edit:
      mCategoryEditor = newName ->
      {
        BookmarkManager.INSTANCE.setCategoryName(mSelectedCategory.getId(), newName);
      };
      EditTextDialogFragment.show(getString(R.string.bookmark_set_name),
                                  mSelectedCategory.getName(),
                                  getString(R.string.rename), getString(R.string.cancel),
                                  MAX_CATEGORY_NAME_LENGTH, this);
      break;
    }

    return true;
  }

/*
  @Override
  public void onLongItemClick(View v, BookmarkCategory category, int position)
  {
    showBottomMenu(category);
  }
*/

  private void showBottomMenu(BookmarkCategory item)
  {
    mSelectedCategory = item;


    BottomSheetHelper.Builder bs = BottomSheetHelper.create(getActivity(), mSelectedCategory.getName())
                                                    .sheet(R.menu.menu_bookmark_categories)
                                                    .listener(this);

    bs.getItemByIndex(0)
      .setIcon(mSelectedCategory.isVisible() ? R.drawable.ic_hide : R.drawable.ic_show)
      .setTitle(mSelectedCategory.isVisible() ? R.string.hide : R.string.show);

    final boolean deleteIsPossible = getAdapter().getBookmarkCategories().size() > 1;
    bs.getItemById(R.id.set_delete)
      .setVisible(deleteIsPossible)
      .setEnabled(deleteIsPossible);

    bs.tint().show();
  }

  @Override
  public void onMoreOperationClick(BookmarkCategory item)
  {
    showBottomMenu(item);
  }

/*
  @Override
  public void onItemClick(View v, BookmarkCategory entity, int position)
  {
    startActivity(new Intent(getActivity(), BookmarkListActivity.class)
                      .putExtra(BookmarksListFragment.EXTRA_CATEGORY, position));
  }
*/

  @Override
  protected void setupPlaceholder(@Nullable PlaceholderView placeholder)
  {
    // A placeholder is no needed on this screen.
  }

  @Override
  public void onBookmarksLoadingStarted()
  {
    updateLoadingPlaceholder();
  }

  @Override
  public void onBookmarksLoadingFinished()
  {
    updateLoadingPlaceholder();
    if (getAdapter() != null)
      getAdapter().notifyDataSetChanged();
    mImportKmlTask.run();
  }

  @Override
  public void onBookmarksFileLoaded(boolean success)
  {
    // Do nothing here.
  }

  private void importKml()
  {
    if (mKmlImportController != null)
      mKmlImportController.importKml();
  }

  @Override
  public void onPreparedFileForSharing(@NonNull BookmarkSharingResult result)
  {
    SharingHelper.INSTANCE.onPreparedFileForSharing(getActivity(), result);
  }

  @Override
  public void onFooterClick()
  {
    mCategoryEditor = BookmarkManager.INSTANCE::createCategory;

    EditTextDialogFragment.show(getString(R.string.bookmarks_create_new_group),
                                getString(R.string.bookmarks_new_list_hint),
                                getString(R.string.bookmark_set_name),
                                getString(R.string.create), getString(R.string.cancel),
                                MAX_CATEGORY_NAME_LENGTH, this);
  }

  @Override
  public void onFinishKmlImport()
  {
    if (getAdapter() != null)
      getAdapter().notifyDataSetChanged();
  }

  @NonNull
  @Override
  public EditTextDialogFragment.OnTextSaveListener getSaveTextListener()
  {
    return text -> {
      if (mCategoryEditor != null)
        mCategoryEditor.commit(text);

      if (getAdapter() != null)
        getAdapter().notifyDataSetChanged();
    };
  }

  @NonNull
  @Override
  public EditTextDialogFragment.Validator getValidator()
  {
    return new CategoryValidator();
  }

  @Override
  public void onItemClick(View v, BookmarkCategory category)
  {
    startActivity(new Intent(getActivity(), BookmarkListActivity.class)
                      .putExtra(BookmarksListFragment.EXTRA_CATEGORY, category));
  }

  @Override
  public void onItemLongClick(View v, BookmarkCategory category)
  {
    showBottomMenu(category);
  }

  interface CategoryEditor
  {
    void commit(@NonNull String newName);
  }

  private class ImportKmlTask implements Runnable
  {
    private boolean alreadyDone = false;

    @Override
    public void run()
    {
      if (alreadyDone)
      {
        return;
      }

      importKml();
      alreadyDone = true;
    }
  }
}
