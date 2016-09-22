package com.mapswithme.maps.bookmarks;

import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.LayoutRes;
import android.support.v7.widget.RecyclerView;
import android.view.MenuItem;
import android.view.View;

import com.mapswithme.maps.R;
import com.mapswithme.maps.base.BaseMwmRecyclerFragment;
import com.mapswithme.maps.bookmarks.data.BookmarkCategory;
import com.mapswithme.maps.bookmarks.data.BookmarkManager;
import com.mapswithme.maps.dialog.EditTextDialogFragment;
import com.mapswithme.maps.widget.recycler.RecyclerClickListener;
import com.mapswithme.maps.widget.recycler.RecyclerLongClickListener;
import com.mapswithme.util.BottomSheetHelper;
import com.mapswithme.util.sharing.SharingHelper;

public class BookmarkCategoriesFragment extends BaseMwmRecyclerFragment
                                     implements EditTextDialogFragment.OnTextSaveListener,
                                                MenuItem.OnMenuItemClickListener,
                                                RecyclerClickListener,
                                                RecyclerLongClickListener
{
  private int mSelectedPosition;

  @Override
  @LayoutRes
  protected int getLayoutRes()
  {
    return R.layout.recycler_default;
  }

  @Override
  protected RecyclerView.Adapter createAdapter()
  {
    return new BookmarkCategoriesAdapter(getActivity());
  }

  @Override
  protected BookmarkCategoriesAdapter getAdapter()
  {
    return (BookmarkCategoriesAdapter) super.getAdapter();
  }

  @Override
  public void onViewCreated(View view, Bundle savedInstanceState)
  {
    super.onViewCreated(view, savedInstanceState);

    getAdapter().setOnClickListener(this);
    getAdapter().setOnLongClickListener(this);
  }

  @Override
  public void onResume()
  {
    super.onResume();
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
    final BookmarkCategory category = BookmarkManager.INSTANCE.getCategory(mSelectedPosition);
    category.setName(text);
    getAdapter().notifyDataSetChanged();
  }

  @Override
  public boolean onMenuItemClick(MenuItem item)
  {
    switch (item.getItemId())
    {
    case R.id.set_show:
      BookmarkManager.INSTANCE.toggleCategoryVisibility(mSelectedPosition);
      getAdapter().notifyDataSetChanged();
      break;

    case R.id.set_share:
      SharingHelper.shareBookmarksCategory(getActivity(), mSelectedPosition);
      break;

    case R.id.set_delete:
      BookmarkManager.INSTANCE.nativeDeleteCategory(mSelectedPosition);
      getAdapter().notifyDataSetChanged();
      break;

    case R.id.set_edit:
      new EditTextDialogFragment.Builder(this)
          .title(R.string.bookmark_set_name)
          .hint(R.string.name)
          .initialText(BookmarkManager.INSTANCE.getCategory(mSelectedPosition).getName())
          .positiveButton(R.string.rename)
          .negativeButton(R.string.cancel)
          .show();
      break;
    }

    return true;
  }

  @Override
  public void onLongItemClick(View v, int position)
  {
    mSelectedPosition = position;

    BookmarkCategory category = BookmarkManager.INSTANCE.getCategory(mSelectedPosition);
    BottomSheetHelper.Builder bs = BottomSheetHelper.create(getActivity(), category.getName())
                                                    .sheet(R.menu.menu_bookmark_categories)
                                                    .listener(this);
    MenuItem show = bs.getMenu().getItem(0);
    show.setIcon(category.isVisible() ? R.drawable.ic_hide
                                      : R.drawable.ic_show);
    show.setTitle(category.isVisible() ? R.string.hide
                                       : R.string.show);
    bs.tint().show();
  }

  @Override
  public void onItemClick(View v, int position)
  {
    startActivity(new Intent(getActivity(), BookmarkListActivity.class)
                      .putExtra(ChooseBookmarkCategoryFragment.CATEGORY_ID, position));
  }
}
