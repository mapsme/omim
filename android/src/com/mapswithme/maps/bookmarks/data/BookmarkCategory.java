package com.mapswithme.maps.bookmarks.data;

import android.content.Context;
import android.content.res.Resources;
import android.os.Parcel;
import android.os.Parcelable;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.text.TextUtils;

import com.mapswithme.maps.R;
import com.mapswithme.maps.bookmarks.BookmarksPageFactory;
import com.mapswithme.maps.content.TypeConverter;

public class BookmarkCategory implements Parcelable
{
  private final long mId;
  @NonNull
  private final String mName;
  @Nullable
  private final Author mAuthor;
  private final int mTracksCount;
  private final int mBookmarksCount;
  private final int mTypeIndex;
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
    mTypeIndex = fromCatalog ? Type.CATALOG.ordinal() : Type.OWNED.ordinal();
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

  public Type getType()
  {
    return Type.values()[mTypeIndex];
  }

  public boolean isFromCatalog()
  {
    return Type.values()[mTypeIndex] == Type.CATALOG;
  }

  public boolean isVisible()
  {
    return mIsVisible;
  }

  public int size()
  {
    return getBookmarksCount() +  getTracksCount();
  }

  public int getPluralsCountTemplate()
  {
    return size() == 0
           ? R.plurals.objects
           : getTracksCount() == 0
             ? R.plurals.places
             : getBookmarksCount() == 0
               ? R.string.tracks
               : R.plurals.objects;
  }

  public static class Author implements Parcelable{

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

    @Override
    public int describeContents()
    {
      return 0;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags)
    {
      dest.writeString(this.mId);
      dest.writeString(this.mName);
    }

    protected Author(Parcel in)
    {
      this.mId = in.readString();
      this.mName = in.readString();
    }

    public static final Creator<Author> CREATOR = new Creator<Author>()
    {
      @Override
      public Author createFromParcel(Parcel source)
      {
        return new Author(source);
      }

      @Override
      public Author[] newArray(int size)
      {
        return new Author[size];
      }
    };
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
    sb.append(", mFromCatalog=").append(isFromCatalog());
    sb.append(", mIsVisible=").append(mIsVisible);
    sb.append('}');
    return sb.toString();
  }

  @Override
  public int describeContents()
  {
    return 0;
  }

  @Override
  public void writeToParcel(Parcel dest, int flags)
  {
    dest.writeLong(this.mId);
    dest.writeString(this.mName);
    dest.writeParcelable(this.mAuthor, flags);
    dest.writeInt(this.mTracksCount);
    dest.writeInt(this.mBookmarksCount);
    dest.writeInt(this.mTypeIndex);
    dest.writeByte(this.mIsVisible ? (byte) 1 : (byte) 0);
  }

  protected BookmarkCategory(Parcel in)
  {
    this.mId = in.readLong();
    this.mName = in.readString();
    this.mAuthor = in.readParcelable(Author.class.getClassLoader());
    this.mTracksCount = in.readInt();
    this.mBookmarksCount = in.readInt();
    this.mTypeIndex = in.readInt();
    this.mIsVisible = in.readByte() != 0;
  }

  public static final Creator<BookmarkCategory> CREATOR = new Creator<BookmarkCategory>()
  {
    @Override
    public BookmarkCategory createFromParcel(Parcel source)
    {
      return new BookmarkCategory(source);
    }

    @Override
    public BookmarkCategory[] newArray(int size)
    {
      return new BookmarkCategory[size];
    }
  };

  public enum Type
  {
    OWNED(
        BookmarksPageFactory.OWNED,
        AbstractCategoriesSnapshot.FilterStrategy.Owned.makeInstance()),
    CATALOG(
        BookmarksPageFactory.CATALOG,
        AbstractCategoriesSnapshot.FilterStrategy.Catalog.makeInstance());

    private BookmarksPageFactory mFactory;
    private AbstractCategoriesSnapshot.FilterStrategy mFilterStrategy;

    Type(@NonNull BookmarksPageFactory pageFactory,
         @NonNull AbstractCategoriesSnapshot.FilterStrategy filterStrategy)
    {
      mFactory = pageFactory;
      mFilterStrategy = filterStrategy;
    }

    @NonNull
    public BookmarksPageFactory getFactory()
    {
      return mFactory;
    }

    @NonNull
    public AbstractCategoriesSnapshot.FilterStrategy getFilterStrategy()
    {
      return mFilterStrategy;
    }
  }
}
