package com.mapswithme.maps.bookmarks.persistence;

import android.arch.persistence.room.ColumnInfo;
import android.arch.persistence.room.Entity;
import android.arch.persistence.room.PrimaryKey;
import android.support.annotation.NonNull;

@Entity(tableName = BookmarkCategoryArchive.TABLE_NAME_BOOKMARK_CATEGORY_ARCHIVE)
public class BookmarkCategoryArchive
{
  public static final String TABLE_NAME_BOOKMARK_CATEGORY_ARCHIVE = "bookmark_category_archive";
  public static final String COL_NAME_EXTERNAL_ID = "external_id";
  public static final String COL_NAME_SERVER_ID = "server_id";

  @PrimaryKey(autoGenerate = true)
  private long mId;
  @ColumnInfo(name = COL_NAME_EXTERNAL_ID)
  private final long mExternalContentProviderId;

  @ColumnInfo(name = COL_NAME_SERVER_ID)
  private final String mServerId;

  public BookmarkCategoryArchive(long externalContentProviderId, @NonNull String serverId)
  {
    mExternalContentProviderId = externalContentProviderId;
    mServerId = serverId;
  }

  public void setId(long id)
  {
    mId = id;
  }

  public long getId()
  {
    return mId;
  }

  public long getExternalContentProviderId()
  {
    return mExternalContentProviderId;
  }

  @NonNull
  public String getServerId()
  {
    return mServerId;
  }

  @Override
  public String toString()
  {
    final StringBuilder sb = new StringBuilder("BookmarkCategoryArchive{");
    sb.append("mId=").append(mId);
    sb.append(", mExternalContentProviderId=").append(mExternalContentProviderId);
    sb.append('}');
    return sb.toString();
  }
}
