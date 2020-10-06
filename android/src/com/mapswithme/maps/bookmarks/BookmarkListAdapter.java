package com.mapswithme.maps.bookmarks;

import android.content.res.Resources;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import com.mapswithme.maps.R;
import com.mapswithme.maps.bookmarks.data.BookmarkCategory;
import com.mapswithme.maps.bookmarks.data.BookmarkInfo;
import com.mapswithme.maps.bookmarks.data.BookmarkManager;
import com.mapswithme.maps.bookmarks.data.SortedBlock;
import com.mapswithme.maps.content.DataSource;
import com.mapswithme.maps.widget.recycler.RecyclerClickListener;
import com.mapswithme.maps.widget.recycler.RecyclerLongClickListener;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class BookmarkListAdapter extends RecyclerView.Adapter<Holders.BaseBookmarkHolder>
{
  // view types
  static final int TYPE_TRACK = 0;
  static final int TYPE_BOOKMARK = 1;
  static final int TYPE_SECTION = 2;

  @NonNull
  private final DataSource<BookmarkCategory> mDataSource;
  @Nullable
  private List<Long> mSearchResults;
  @Nullable
  private List<SortedBlock> mSortedResults;

  @SuppressWarnings("NullableProblems")
  @NonNull
  private SectionsDataSource mSectionsDataSource;

  @Nullable
  private RecyclerClickListener mMoreListener;
  @Nullable
  private RecyclerClickListener mClickListener;
  @Nullable
  private RecyclerLongClickListener mLongClickListener;

  public static abstract class SectionsDataSource
  {
    @NonNull
    private final DataSource<BookmarkCategory> mDataSource;

    SectionsDataSource(@NonNull DataSource<BookmarkCategory> dataSource)
    {
      mDataSource = dataSource;
    }

    public BookmarkCategory getCategory() { return mDataSource.getData(); }

    public abstract int getSectionsCount();
    public abstract boolean isEditable(int sectionIndex);
    public abstract boolean hasTitle(int sectionIndex);
    @Nullable
    public abstract String getTitle(int sectionIndex, @NonNull Resources rs);
    public abstract int getItemsCount(int sectionIndex);
    public abstract int getItemsType(int sectionIndex);
    public abstract long getBookmarkId(@NonNull SectionPosition pos);
    public abstract long getTrackId(@NonNull SectionPosition pos);
    public abstract void onDelete(@NonNull SectionPosition pos);
  }

  private static class CategorySectionsDataSource extends SectionsDataSource
  {
    private int mSectionsCount;
    private int mBookmarksSectionIndex;
    private int mTracksSectionIndex;

    CategorySectionsDataSource(@NonNull DataSource<BookmarkCategory> dataSource)
    {
      super(dataSource);
      calculateSections();
    }

    private void calculateSections()
    {
      mBookmarksSectionIndex = SectionPosition.INVALID_POSITION;
      mTracksSectionIndex = SectionPosition.INVALID_POSITION;

      mSectionsCount = 0;
      if (getCategory().getTracksCount() > 0)
        mTracksSectionIndex = mSectionsCount++;
      if (getCategory().getBookmarksCount() > 0)
        mBookmarksSectionIndex = mSectionsCount++;
    }

    @Override
    public int getSectionsCount() { return mSectionsCount; }

    @Override
    public boolean isEditable(int sectionIndex)
    {
      return !getCategory().isFromCatalog();
    }

    @Override
    public boolean hasTitle(int sectionIndex) { return true; }

    @Nullable
    public String getTitle(int sectionIndex, @NonNull Resources rs)
    {
      if (sectionIndex == mTracksSectionIndex)
        return rs.getString(R.string.tracks_title);
      return rs.getString(R.string.bookmarks);
    }

    @Override
    public int getItemsCount(int sectionIndex)
    {
      if (sectionIndex == mTracksSectionIndex)
        return getCategory().getTracksCount();
      if (sectionIndex == mBookmarksSectionIndex)
        return getCategory().getBookmarksCount();
      return 0;
    }

    @Override
    public int getItemsType(int sectionIndex)
    {
      if (sectionIndex == mTracksSectionIndex)
        return TYPE_TRACK;
      if (sectionIndex == mBookmarksSectionIndex)
        return TYPE_BOOKMARK;
      throw new AssertionError("Invalid section index: " + sectionIndex);
    }

    @Override
    public void onDelete(@NonNull SectionPosition pos)
    {
      calculateSections();
    }

    @Override
    public long getBookmarkId(@NonNull SectionPosition pos)
    {
      return BookmarkManager.INSTANCE.getBookmarkIdByPosition(getCategory().getId(),
                                                              pos.getItemIndex());
    }

    @Override
    public long getTrackId(@NonNull SectionPosition pos)
    {
      return BookmarkManager.INSTANCE.getTrackIdByPosition(getCategory().getId(),
                                                           pos.getItemIndex());
    }
  }

  private static class SearchResultsSectionsDataSource extends SectionsDataSource
  {
    @NonNull
    private final List<Long> mSearchResults;

    SearchResultsSectionsDataSource(@NonNull DataSource<BookmarkCategory> dataSource,
                                    @NonNull List<Long> searchResults)
    {
      super(dataSource);
      mSearchResults = searchResults;
    }

    @Override
    public int getSectionsCount() { return 1; }

    @Override
    public boolean isEditable(int sectionIndex) { return true; }

    @Override
    public boolean hasTitle(int sectionIndex) { return false; }

    @Nullable
    public String getTitle(int sectionIndex, @NonNull Resources rs) { return null; }

    @Override
    public int getItemsCount(int sectionIndex) { return mSearchResults.size(); }

    @Override
    public int getItemsType(int sectionIndex) { return TYPE_BOOKMARK; }

    @Override
    public void onDelete(@NonNull SectionPosition pos)
    {
      mSearchResults.remove(pos.getItemIndex());
    }

    @Override
    public long getBookmarkId(@NonNull SectionPosition pos)
    {
      return mSearchResults.get(pos.getItemIndex());
    }

    @Override
    public long getTrackId(@NonNull SectionPosition pos)
    {
      throw new AssertionError("Tracks unsupported in search results.");
    }
  }

  private static class SortedSectionsDataSource extends SectionsDataSource
  {
    @NonNull
    private List<SortedBlock> mSortedBlocks;

    SortedSectionsDataSource(@NonNull DataSource<BookmarkCategory> dataSource,
                             @NonNull List<SortedBlock> sortedBlocks)
    {
      super(dataSource);
      mSortedBlocks = sortedBlocks;
    }


    @NonNull
    private SortedBlock getSortedBlock(int sectionIndex)
    {
      return mSortedBlocks.get(sectionIndex);
    }

    @Override
    public int getSectionsCount()
    {
      return mSortedBlocks.size();
    }

    @Override
    public boolean isEditable(int sectionIndex)
    {
      return true;
    }

    @Override
    public boolean hasTitle(int sectionIndex) { return true; }

    @Nullable
    public String getTitle(int sectionIndex, @NonNull Resources rs)
    {
      return getSortedBlock(sectionIndex).getName();
    }

    @Override
    public int getItemsCount(int sectionIndex)
    {
      SortedBlock block = getSortedBlock(sectionIndex);
      if (block.isBookmarksBlock())
        return block.getBookmarkIds().size();
      return block.getTrackIds().size();
    }

    @Override
    public int getItemsType(int sectionIndex)
    {
      if (getSortedBlock(sectionIndex).isBookmarksBlock())
        return TYPE_BOOKMARK;
      return TYPE_TRACK;
    }

    @Override
    public void onDelete(@NonNull SectionPosition pos)
    {
      int blockIndex = pos.getSectionIndex();
      SortedBlock block = mSortedBlocks.get(blockIndex);
      if (block.isBookmarksBlock())
      {
        block.getBookmarkIds().remove(pos.getItemIndex());
        if (block.getBookmarkIds().isEmpty())
          mSortedBlocks.remove(blockIndex);
        return;
      }

      block.getTrackIds().remove(pos.getItemIndex());
      if (block.getTrackIds().isEmpty())
        mSortedBlocks.remove(blockIndex);
    }

    public long getBookmarkId(@NonNull SectionPosition pos)
    {
      return getSortedBlock(pos.getSectionIndex()).getBookmarkIds().get(pos.getItemIndex());
    }

    public long getTrackId(@NonNull SectionPosition pos)
    {
      return getSortedBlock(pos.getSectionIndex()).getTrackIds().get(pos.getItemIndex());
    }
  }

  BookmarkListAdapter(@NonNull DataSource<BookmarkCategory> dataSource)
  {
    mDataSource = dataSource;
    refreshSections();
  }

  private void refreshSections()
  {
    if (mSearchResults != null)
      mSectionsDataSource = new SearchResultsSectionsDataSource(mDataSource, mSearchResults);
    else if (mSortedResults != null)
      mSectionsDataSource = new SortedSectionsDataSource(mDataSource, mSortedResults);
    else
      mSectionsDataSource = new CategorySectionsDataSource(mDataSource);
  }

  private SectionPosition getSectionPosition(int position)
  {
    int startSectionRow = 0;
    boolean hasTitle;
    int sectionsCount = mSectionsDataSource.getSectionsCount();
    for (int i = 0; i < sectionsCount; ++i)
    {
      hasTitle = mSectionsDataSource.hasTitle(i);
      int sectionRowsCount = mSectionsDataSource.getItemsCount(i) + (hasTitle ? 1 : 0);
      if (startSectionRow == position && hasTitle)
        return new SectionPosition(i, SectionPosition.INVALID_POSITION);
      if (startSectionRow + sectionRowsCount > position)
        return new SectionPosition(i, position - startSectionRow - (hasTitle ? 1 : 0));
      startSectionRow += sectionRowsCount;
    }
    return new SectionPosition(SectionPosition.INVALID_POSITION, SectionPosition.INVALID_POSITION);
  }

  void setSearchResults(@Nullable long[] searchResults)
  {
    if (searchResults != null)
    {
      mSearchResults = new ArrayList<Long>(searchResults.length);
      for (long id : searchResults)
        mSearchResults.add(id);
    }
    else
    {
      mSearchResults = null;
    }
    refreshSections();
  }

  void setSortedResults(@Nullable SortedBlock[] sortedResults)
  {
    if (sortedResults != null)
      mSortedResults = new ArrayList<>(Arrays.asList(sortedResults));
    else
      mSortedResults = null;
    refreshSections();
  }

  public void setOnClickListener(@Nullable RecyclerClickListener listener)
  {
    mClickListener = listener;
  }

  void setOnLongClickListener(@Nullable RecyclerLongClickListener listener)
  {
    mLongClickListener = listener;
  }

  void setMoreListener(@Nullable RecyclerClickListener listener)
  {
    mMoreListener = listener;
  }

  @Override
  @NonNull
  public Holders.BaseBookmarkHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType)
  {
    LayoutInflater inflater = LayoutInflater.from(parent.getContext());
    Holders.BaseBookmarkHolder holder = null;
    switch (viewType)
    {
      case TYPE_TRACK:
        Holders.TrackViewHolder trackHolder =
            new Holders.TrackViewHolder(inflater.inflate(R.layout.item_track, parent,
                                                         false));
        trackHolder.setOnClickListener(mClickListener);
        holder = trackHolder;
        break;
      case TYPE_BOOKMARK:
        Holders.BookmarkViewHolder bookmarkHolder =
            new Holders.BookmarkViewHolder(inflater.inflate(R.layout.item_bookmark, parent,
                                                            false));
        bookmarkHolder.setOnClickListener(mClickListener);
        bookmarkHolder.setOnLongClickListener(mLongClickListener);
        bookmarkHolder.setMoreListener(mMoreListener);
        holder = bookmarkHolder;
        break;
      case TYPE_SECTION:
        TextView tv = (TextView) inflater.inflate(R.layout.item_category_title, parent, false);
        holder = new Holders.SectionViewHolder(tv);
        break;
    }

    if (holder == null)
      throw new AssertionError("Unsupported view type: " + viewType);

    return holder;
  }

  @Override
  public void onBindViewHolder(@NonNull Holders.BaseBookmarkHolder holder, int position)
  {
    SectionPosition sp = getSectionPosition(position);
    holder.bind(sp, mSectionsDataSource);
  }

  @Override
  public int getItemViewType(int position)
  {
    SectionPosition sp = getSectionPosition(position);
    if (sp.isTitlePosition())
      return TYPE_SECTION;
    if (sp.isItemPosition())
      return mSectionsDataSource.getItemsType(sp.getSectionIndex());
    throw new IllegalArgumentException("Position not found: " + position);
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
    int sectionsCount = mSectionsDataSource.getSectionsCount();
    for (int i = 0; i < sectionsCount; ++i)
    {
      int sectionItemsCount = mSectionsDataSource.getItemsCount(i);
      if (sectionItemsCount == 0)
        continue;
      itemCount += sectionItemsCount;
      if (mSectionsDataSource.hasTitle(i))
        ++itemCount;
    }
    return itemCount;
  }

  void onDelete(int position)
  {
    SectionPosition sp = getSectionPosition(position);
    mSectionsDataSource.onDelete(sp);
    // In case of the search results editing reset cached sorted blocks.
    if (isSearchResults())
      mSortedResults = null;
  }

  boolean isSearchResults()
  {
    return mSearchResults != null;
  }

  public Object getItem(int position)
  {
    SectionPosition pos = getSectionPosition(position);
    if (getItemViewType(position) == TYPE_TRACK)
    {
      final long trackId = mSectionsDataSource.getTrackId(pos);
      return BookmarkManager.INSTANCE.getTrack(trackId);
    }
    else
    {
      final long bookmarkId = mSectionsDataSource.getBookmarkId(pos);
      BookmarkInfo info = BookmarkManager.INSTANCE.getBookmarkInfo(bookmarkId);
      if (info == null)
        throw new RuntimeException("Bookmark no longer exists " + bookmarkId);
      return info;
    }
  }
}
