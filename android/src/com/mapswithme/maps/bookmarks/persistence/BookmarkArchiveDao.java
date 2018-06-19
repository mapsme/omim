package com.mapswithme.maps.bookmarks.persistence;

import android.arch.persistence.room.Dao;
import android.arch.persistence.room.Delete;
import android.arch.persistence.room.Insert;
import android.arch.persistence.room.OnConflictStrategy;
import android.arch.persistence.room.Query;
import android.support.annotation.Nullable;

import java.util.List;

@Dao
public interface BookmarkArchiveDao
{
  @Insert(onConflict = OnConflictStrategy.REPLACE)
  long createOrUpdate(BookmarkCategoryArchive archive);

  @Query("SELECT * FROM " + BookmarkCategoryArchive
      .TABLE_NAME_BOOKMARK_CATEGORY_ARCHIVE)
  List<BookmarkCategoryArchive> getAll();

  @Delete
  void delete(BookmarkCategoryArchive... archives);

  @Query("DELETE FROM " + BookmarkCategoryArchive
      .TABLE_NAME_BOOKMARK_CATEGORY_ARCHIVE + " WHERE " + BookmarkCategoryArchive.COL_NAME_SERVER_ID + " = :serverId")
  int deleteByServerId(String serverId);

  @Nullable
  @Query("SELECT * FROM " + BookmarkCategoryArchive.TABLE_NAME_BOOKMARK_CATEGORY_ARCHIVE + " " +
         "WHERE " + BookmarkCategoryArchive.COL_NAME_SERVER_ID + " = :serverId LIMIT 1")
  BookmarkCategoryArchive getArchive(String serverId);
}
