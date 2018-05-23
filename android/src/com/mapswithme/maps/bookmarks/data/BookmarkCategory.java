package com.mapswithme.maps.bookmarks.data;

import android.content.Context;
import android.content.res.Resources;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.text.TextUtils;

import com.mapswithme.maps.R;
import com.mapswithme.maps.content.TypeConverter;

public class BookmarkCategory
{
  private final long mId;
  @NonNull
  private final String mName;
  @Nullable
  private final Author mAuthor;
  private final int mTracksCount;
  private final int mBookmarksCount;
  private final boolean mFromCatalog;
  private final boolean mIsVisible;

  public BookmarkCategory(long id,
                          @NonNull String name,
                          @NonNull String authorId,
                          @NonNull String authorName,
                          int tracksCount,
                          int bookmarksCount,
                          boolean fromCatalog,
                          boolean isVisible)
  {
    mId = id;
    mName = name;
    mTracksCount = tracksCount;
    mBookmarksCount = bookmarksCount;
    mFromCatalog = fromCatalog;
    mIsVisible = isVisible;
    mAuthor = TextUtils.isEmpty(authorId) || TextUtils.isEmpty(authorName)
              ? null
              : new Author(authorId, authorName);
  }


  @Override
  public boolean equals(Object o)
  {
    if (this == o) return true;
    if (o == null || getClass() != o.getClass()) return false;
    BookmarkCategory that = (BookmarkCategory) o;
    return mId == that.mId;
  }

  @Override
  public int hashCode()
  {
    return (int)(mId ^ (mId >>> 32));
  }

  public long getId()
  {
    return mId;
  }

  @NonNull
  public String getName()
  {
    return mName;
  }

  @Nullable
  public Author getAuthor()
  {
    return mAuthor;
  }

  public int getTracksCount()
  {
    return mTracksCount;
  }

  public int getBookmarksCount()
  {
    return mBookmarksCount;
  }

  public boolean isFromCatalog()
  {
    return mFromCatalog;
  }

  public boolean isVisible()
  {
    return mIsVisible;
  }

  public int size()
  {
    return getBookmarksCount() +  getTracksCount();
  }

  public static class Author {

    public static final String PHRASE_SEPARATOR = " • ";
    public static final String SPACE = " ";
    @NonNull
    private final String mId;
    @NonNull
    private final String mName;

    public Author(@NonNull String id, @NonNull String name)
    {
      mId = id;
      mName = name;
    }

    @NonNull
    public String getId()
    {
      return mId;
    }

    @NonNull
    public String getName()
    {
      return mName;
    }


    public static String getRepresentation(Context context, Author author){
      Resources res = context.getResources();
      return new StringBuilder()
          .append(PHRASE_SEPARATOR)
          .append(String.format(res.getString(R.string.author_name_by_prefix),
                                author.getName()))
          .toString();
    }

    @Override
    public String toString()
    {
      final StringBuilder sb = new StringBuilder("Author{");
      sb.append("mId='").append(mId).append('\'');
      sb.append(", mName='").append(mName).append('\'');
      sb.append('}');
      return sb.toString();
    }

    @Override
    public boolean equals(Object o)
    {
      if (this == o) return true;
      if (o == null || getClass() != o.getClass()) return false;
      Author author = (Author) o;
      return mId.equals(author.mId);
    }

    @Override
    public int hashCode()
    {
      return mId.hashCode();
    }
  }

  public static class IsFromCatalog implements TypeConverter<BookmarkCategory, Boolean>
  {
    @Override
    public Boolean convert(@NonNull BookmarkCategory data)
    {
      return data.isFromCatalog();
    }
  }

  @Override
  public String toString()
  {
    final StringBuilder sb = new StringBuilder("BookmarkCategory{");
    sb.append("mId=").append(mId);
    sb.append(", mName='").append(mName).append('\'');
    sb.append(", mAuthor=").append(mAuthor);
    sb.append(", mTracksCount=").append(mTracksCount);
    sb.append(", mBookmarksCount=").append(mBookmarksCount);
    sb.append(", mFromCatalog=").append(mFromCatalog);
    sb.append(", mIsVisible=").append(mIsVisible);
    sb.append('}');
    return sb.toString();
  }
}
