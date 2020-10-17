package com.mapswithme.maps.bookmarks;

import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;
import com.mapswithme.maps.R;
import com.mapswithme.maps.adapter.OnItemClickListener;
import com.mapswithme.maps.bookmarks.data.BookmarkCategory;
import com.mapswithme.maps.bookmarks.data.BookmarkManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

public class BookmarkCollectionAdapter extends RecyclerView.Adapter<RecyclerView.ViewHolder>
{
  @Retention(RetentionPolicy.SOURCE)
  @IntDef({ TYPE_HEADER_ITEM, TYPE_COLLECTION_ITEM, TYPE_CATEGORY_ITEM })
  public @interface SectionType { }

  private final static int TYPE_COLLECTION_ITEM = BookmarkManager.COLLECTION;
  private final static int TYPE_CATEGORY_ITEM = BookmarkManager.CATEGORY;
  private final static int TYPE_HEADER_ITEM = 3;

  private final long mParentCategoryId;

  @NonNull
  private List<BookmarkCategory> mItemsCollection;
  @NonNull
  private List<BookmarkCategory> mItemsCategory;

  private int mSectionCount;
  private int mCollectionSectionIndex = SectionPosition.INVALID_POSITION;
  private int mCategorySectionIndex = SectionPosition.INVALID_POSITION;

  @Nullable
  private OnItemClickListener<BookmarkCategory> mClickListener;
  @NonNull
  private final MassOperationAction mMassOperationAction = new MassOperationAction();

  private class ToggleVisibilityClickListener implements View.OnClickListener
  {
    @NonNull
    private final Holders.CollectionViewHolder mHolder;

    ToggleVisibilityClickListener(@NonNull Holders.CollectionViewHolder holder)
    {
      mHolder = holder;
    }

    @Override
    public void onClick(View v)
    {
      BookmarkCategory category = mHolder.getEntity();
      BookmarkManager.INSTANCE.toggleCategoryVisibility(category);
      category.invertVisibility();
      notifyItemChanged(mHolder.getAdapterPosition());
      notifyItemChanged(SectionPosition.INVALID_POSITION);
    }
  }

  BookmarkCollectionAdapter(long parentCategoryId,
                            @NonNull List<BookmarkCategory> itemsCategories,
                            @NonNull List<BookmarkCategory> itemsCollection)
  {
    mParentCategoryId = parentCategoryId;
    //noinspection AssignmentOrReturnOfFieldWithMutableType
    mItemsCategory = itemsCategories;
    //noinspection AssignmentOrReturnOfFieldWithMutableType
    mItemsCollection = itemsCollection;

    mSectionCount = 0;
    if (mItemsCollection.size() > 0)
      mCollectionSectionIndex = mSectionCount++;
    if (mItemsCategory.size() > 0)
      mCategorySectionIndex = mSectionCount++;
  }

  public String getTitle(int sectionIndex, @NonNull Resources rs)
  {
    if (sectionIndex == mCollectionSectionIndex)
      // TODO (@velichkomarija): Replace categories for collections.
      return rs.getString(R.string.categories);
    return rs.getString(R.string.categories);
  }

  public int getItemsCount(int sectionIndex)
  {
    if (sectionIndex == mCollectionSectionIndex)
      return mItemsCollection.size();
    if (sectionIndex == mCategorySectionIndex)
      return mItemsCategory.size();
    return 0;
  }

  @SectionType
  public int getItemsType(int sectionIndex)
  {
    if (sectionIndex == mCollectionSectionIndex)
      return TYPE_COLLECTION_ITEM;
    if (sectionIndex == mCategorySectionIndex)
      return TYPE_CATEGORY_ITEM;
    throw new AssertionError("Invalid section index: " + sectionIndex);
  }

  @NonNull
  private List<BookmarkCategory> getItemsListByType(@SectionType int type)
  {
    if (type == TYPE_COLLECTION_ITEM)
      return mItemsCollection;
    else
      return mItemsCategory;
  }

  @NonNull
  public BookmarkCategory getGroupByPosition(SectionPosition sp, @SectionType int type)
  {
    List<BookmarkCategory> categories = getItemsListByType(type);

    int itemIndex = sp.getItemIndex();
    if (sp.getItemIndex() > categories.size() - 1)
      throw new ArrayIndexOutOfBoundsException(itemIndex);
    return categories.get(itemIndex);
  }

  public void setOnClickListener(@Nullable OnItemClickListener<BookmarkCategory> listener)
  {
    mClickListener = listener;
  }

  private SectionPosition getSectionPosition(int position)
  {
    int startSectionRow = 0;
    for (int i = 0; i < mSectionCount; ++i)
    {
      int sectionRowsCount = getItemsCount(i) + /* header */ 1;
      if (startSectionRow == position)
        return new SectionPosition(i, SectionPosition.INVALID_POSITION);
      if (startSectionRow + sectionRowsCount > position)
        return new SectionPosition(i, position - startSectionRow - /* header */ 1);
      startSectionRow += sectionRowsCount;
    }
    return new SectionPosition(SectionPosition.INVALID_POSITION, SectionPosition.INVALID_POSITION);
  }

  @NonNull
  @Override
  public RecyclerView.ViewHolder onCreateViewHolder(@NonNull ViewGroup parent,
                                                    @SectionType int viewType)
  {
    LayoutInflater inflater = LayoutInflater.from(parent.getContext());
    RecyclerView.ViewHolder holder = null;

    if (viewType == TYPE_HEADER_ITEM)
      holder = new Holders.HeaderViewHolder(inflater.inflate(R.layout.item_bookmark_group_list_header,
                                                             parent, false));
    if (viewType == TYPE_CATEGORY_ITEM || viewType == TYPE_COLLECTION_ITEM)
      holder = new Holders.CollectionViewHolder(inflater.inflate(R.layout.item_bookmark_collection,
                                                                 parent, false));

    if (holder == null)
      throw new AssertionError("Unsupported view type: " + viewType);

    return holder;
  }

  @Override
  public void onBindViewHolder(@NonNull RecyclerView.ViewHolder holder, int position)
  {
    SectionPosition sectionPosition = getSectionPosition(position);
    if (sectionPosition.isTitlePosition())
      bindHeaderHolder(holder, sectionPosition.getSectionIndex());
    else
      bindCollectionHolder(holder, sectionPosition, getItemsType(sectionPosition.getSectionIndex()));
  }

  @Override
  @SectionType
  public int getItemViewType(int position)
  {
    SectionPosition sectionPosition = getSectionPosition(position);
    if (sectionPosition.isTitlePosition())
      return TYPE_HEADER_ITEM;
    if (sectionPosition.isItemPosition())
      return getItemsType(sectionPosition.getSectionIndex());
    throw new AssertionError("Position not found: " + position);
  }

  private void bindCollectionHolder(RecyclerView.ViewHolder holder, SectionPosition position,
                                    @SectionType int type)
  {
    final BookmarkCategory category = getGroupByPosition(position, type);
    Holders.CollectionViewHolder collectionViewHolder = (Holders.CollectionViewHolder) holder;
    collectionViewHolder.setEntity(category);
    collectionViewHolder.setName(category.getName());
    bindSize(collectionViewHolder, category);
    collectionViewHolder.setVisibilityState(category.isVisible());
    collectionViewHolder.setOnClickListener(mClickListener);
    ToggleVisibilityClickListener listener = new ToggleVisibilityClickListener(collectionViewHolder);
    collectionViewHolder.setVisibilityListener(listener);
  }

  private void bindSize(Holders.CollectionViewHolder holder, BookmarkCategory category)
  {
    BookmarkCategory.CountAndPlurals template = category.getPluralsCountTemplate();
    holder.setSize(template.getPlurals(), template.getCount());
  }

  private void bindHeaderHolder(@NonNull RecyclerView.ViewHolder holder, int nextSectionPosition)
  {
    Holders.HeaderViewHolder headerViewHolder = (Holders.HeaderViewHolder) holder;
    headerViewHolder.getText()
                    .setText(getTitle(nextSectionPosition, holder.itemView.getResources()));

    int compilationType = nextSectionPosition == mCategorySectionIndex ?
                          BookmarkManager.CATEGORY : BookmarkManager.COLLECTION;
    headerViewHolder.setAction(mMassOperationAction,
                               !isCategoryAllItemsAreVisible(nextSectionPosition), compilationType);
  }

  @Override
  public long getItemId(int position)
  {
    return position;
  }

  @Override
  public int getItemCount()
  {
    int itemCount = 0;

    for (int i = 0; i < mSectionCount; ++i)
    {
      int sectionItemsCount = getItemsCount(i);
      if (sectionItemsCount == 0)
        continue;
      itemCount += sectionItemsCount + /* header */ 1;
    }
    return itemCount;
  }

  // TODO (@velichkomarija): Remove this method in core.
  private boolean isCategoryAllItemsAreVisible(int sectionPosition)
  {
    int type = sectionPosition == mCategorySectionIndex ? TYPE_CATEGORY_ITEM : TYPE_COLLECTION_ITEM;
    for (BookmarkCategory category : getItemsListByType(type))
    {
      if (!category.isVisible())
        return false;
    }
    return true;
  }

  private void updateAllVisibility(@BookmarkManager.CompilationType int compilationType)
  {
    if (compilationType == BookmarkManager.COLLECTION)
      mItemsCollection = BookmarkManager.INSTANCE.getChildrenCollections(mParentCategoryId);
    else
      mItemsCategory = BookmarkManager.INSTANCE.getChildrenCategories(mParentCategoryId);
  }

  class MassOperationAction implements Holders.HeaderViewHolder.HeaderActionChildCategories
  {
    @Override
    public void onHideAll(@BookmarkManager.CompilationType int compilationType)
    {
      BookmarkManager.INSTANCE.setChildCategoriesVisibility(mParentCategoryId, compilationType, false);
      updateAllVisibility(compilationType);
      notifyDataSetChanged();
    }

    @Override
    public void onShowAll(@BookmarkManager.CompilationType int compilationType)
    {
      BookmarkManager.INSTANCE.setChildCategoriesVisibility(mParentCategoryId, compilationType, true);
      updateAllVisibility(compilationType);
      notifyDataSetChanged();
    }
  }
}

