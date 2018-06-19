package com.mapswithme.maps;

import android.arch.persistence.room.Database;
import android.arch.persistence.room.RoomDatabase;
import android.content.Context;
import android.support.annotation.NonNull;

import com.mapswithme.maps.bookmarks.persistence.BookmarkCategoryArchive;
import com.mapswithme.maps.bookmarks.persistence.BookmarkArchiveDao;

@Database(entities = { BookmarkCategoryArchive.class }, version = MwmAppDb.DB_VERSION, exportSchema = false)
public abstract class MwmAppDb extends RoomDatabase
{
  public static final int DB_VERSION = 1;

  public abstract BookmarkArchiveDao bookmarkArchiveDao();

  public static MwmAppDb from(@NonNull Context context)
  {
    MwmApplication app = (MwmApplication) context.getApplicationContext();
    return app.getAppDb();
  }
}
