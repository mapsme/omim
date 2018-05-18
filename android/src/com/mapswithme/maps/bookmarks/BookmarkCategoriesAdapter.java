package com.mapswithme.maps.bookmarks;

import android.content.Context;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import com.mapswithme.maps.R;
import com.mapswithme.maps.bookmarks.data.BookmarkManager;
import com.mapswithme.maps.widget.recycler.RecyclerClickListener;
import com.mapswithme.maps.widget.recycler.RecyclerLongClickListener;

import static com.mapswithme.maps.bookmarks.Holders.CategoryViewHolder;
import static com.mapswithme.maps.bookmarks.Holders.HeaderViewHolder;

public class BookmarkCategoriesAdapter extends BaseBookmarkCategoryAdapter<RecyclerView.ViewHolder>
{
  private final static int TYPE_CATEGORY_ITEM = 0;
  private final static int TYPE_ACTION_FOOTER = 1;
  private final static int TYPE_ACTION_HEADER = 2;
  private final static int HEADER_POSITION = 0;
  @NonNull
  private final AbstractAdapterResourceProvider mResProvider;
  @Nullable
  private RecyclerLongClickListener mLongClickListener;
  @Nullable
  private RecyclerClickListener mClickListener;
  @Nullable
  private CategoryListCallback mCategoryListCallback;
  @NonNull
  private final MassOperationAction mMassOperationAction;

  BookmarkCategoriesAdapter(@NonNull Context context,
                            @NonNull AbstractAdapterResourceProvider resProvider)
  {
    super(context.getApplicationContext());
    mResProvider = resProvider;
    mMassOperationAction = new MassOperationAction();
  }

  BookmarkCategoriesAdapter(@NonNull Context context)
  {
    this(context, BookmarksPageFactory.OWNED.getResProvider());
  }

  public void setOnClickListener(@Nullable RecyclerClickListener listener)
  {
    mClickListener = listener;
  }

  void setOnLongClickListener(@Nullable RecyclerLongClickListener listener)
  {
    mLongClickListener = listener;
  }

  void setCategoryListCallback(@Nullable CategoryListCallback listener)
  {
    mCategoryListCallback = listener;
  }

  @Override
  public RecyclerView.ViewHolder onCreateViewHolder(ViewGroup parent, int viewType)
  {
    LayoutInflater inflater = LayoutInflater.from(parent.getContext());
    if (viewType == TYPE_ACTION_HEADER)
    {
      View header = inflater.inflate(R.layout.item_bookmark_group_list_header, parent, false);
      return new Holders.HeaderViewHolder(header);
    }

    if (viewType == TYPE_ACTION_FOOTER)
    {
      View item = inflater.inflate(R.layout.item_bookmark_create_group, parent, false);
      item.setOnClickListener(new FooterClickListener());
      return new Holders.GeneralViewHolder(item);
    }

    View view = inflater.inflate(R.layout.item_bookmark_category,
                                 parent,
                                 false);
    final CategoryViewHolder holder = new CategoryViewHolder(view);
    view.setOnClickListener(new CategoryItemClickListener(holder));
    view.setOnLongClickListener(new LongClickListener(view, holder));

    return holder;
  }

  @Override
  public void onBindViewHolder(final RecyclerView.ViewHolder holder, final int position)
  {
    int type = getItemViewType(position);
    if (type == TYPE_ACTION_FOOTER)
    {
      Holders.GeneralViewHolder generalHolder = (Holders.GeneralViewHolder) holder;
      generalHolder.getImage().setImageResource(mResProvider.getFooterImage());
      generalHolder.getText().setText(mResProvider.getFooterText());
      return;
    }

    if (type == TYPE_ACTION_HEADER)
    {
      HeaderViewHolder headerHolder = (HeaderViewHolder) holder;
      headerHolder.setAction(mMassOperationAction,
                             mResProvider, BookmarkManager.INSTANCE.areAllCategoriesInvisible()
                            );
      headerHolder.getText().setText(mResProvider.getHeaderText());
      return;
    }

    CategoryViewHolder categoryHolder = (CategoryViewHolder) holder;
    final BookmarkManager bmManager = BookmarkManager.INSTANCE;
    final long catId = getCategoryIdByPosition(toCategoryPosition(position));
    categoryHolder.setName(bmManager.getCategoryName(catId));
    categoryHolder.setSize(bmManager.getCategorySize(catId));
    categoryHolder.setVisibilityState(bmManager.isVisible(catId));
    categoryHolder.setVisibilityListener(new ToggleVisibilityClickListener(catId, categoryHolder, bmManager));
    categoryHolder.setMoreListener(v -> {
      if (mCategoryListCallback != null)
        mCategoryListCallback.onMoreOperationClick(toCategoryPosition(position));
    });
  }

  @Override
  public int getItemViewType(int position)
  {
    if (position == 0)
      return TYPE_ACTION_HEADER;
    return (position == getItemCount() - 1) ? TYPE_ACTION_FOOTER : TYPE_CATEGORY_ITEM;
  }

  private int toCategoryPosition(int adapterPosition)
  {

    int type = getItemViewType(adapterPosition);
    if (type != TYPE_CATEGORY_ITEM)
      throw new AssertionError("An element at specified position is not category!");

    // The header "Hide All" is located at first index, so subtraction is needed.
    return adapterPosition - 1;
  }

  @Override
  public int getItemCount()
  {
    int count = super.getItemCount();
    return count > 0 ? count + 2 /* header + add category btn */ : 0;
  }

  private class LongClickListener implements View.OnLongClickListener
  {
    private final View mView;
    private final CategoryViewHolder mHolder;

    public LongClickListener(View view, CategoryViewHolder holder)
    {
      mView = view;
      mHolder = holder;
    }

    @Override
    public boolean onLongClick(View view)
    {
      if (mLongClickListener != null)
        mLongClickListener.onLongItemClick(mView, toCategoryPosition(mHolder.getAdapterPosition()));
      return true;
    }
  }

  private class MassOperationAction implements HeaderViewHolder.HeaderAction
  {
    @Override
    public void onHideAll()
    {
      BookmarkManager.INSTANCE.setAllCategoriesVisibility(false);
      notifyDataSetChanged();
    }

    @Override
    public void onShowAll()
    {
      BookmarkManager.INSTANCE.setAllCategoriesVisibility(true);
      notifyDataSetChanged();
    }
  }

  private class CategoryItemClickListener implements View.OnClickListener
  {
    private final CategoryViewHolder mHolder;

    public CategoryItemClickListener(CategoryViewHolder holder)
    {
      mHolder = holder;
    }

    @Override
    public void onClick(View v)
    {
      if (mClickListener != null)
      {
        final int pos = mHolder.getAdapterPosition();
        mClickListener.onItemClick(v, BookmarkCategoriesAdapter.this.toCategoryPosition(pos));
      }
    }
  }

  private class FooterClickListener implements View.OnClickListener
  {
    @Override
    public void onClick(View v)
    {
      if (mCategoryListCallback != null)
        mCategoryListCallback.onFooterClick();
    }
  }

  private class ToggleVisibilityClickListener implements View.OnClickListener
  {
    private final long mCatId;
    private final CategoryViewHolder mCategoryHolder;
    private final BookmarkManager mBmManager;

    public ToggleVisibilityClickListener(long catId, CategoryViewHolder categoryHolder,
                                         BookmarkManager bmManager)
    {
      mCatId = catId;
      mCategoryHolder = categoryHolder;
      mBmManager = bmManager;
    }

    @Override
    public void onClick(View v)
    {
      BookmarkManager.INSTANCE.toggleCategoryVisibility(mCatId);
      mCategoryHolder.setVisibilityState(mBmManager.isVisible(mCatId));
      notifyItemChanged(HEADER_POSITION);
    }
  }
}
