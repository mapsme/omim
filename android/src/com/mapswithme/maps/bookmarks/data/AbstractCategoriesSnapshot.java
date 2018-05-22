package com.mapswithme.maps.bookmarks.data;

import android.support.annotation.NonNull;

import com.mapswithme.maps.content.Predicate;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

public class AbstractCategoriesSnapshot
{
  @NonNull
  private final List<BookmarkCategory> mSnapshot;

  public AbstractCategoriesSnapshot(@NonNull BookmarkCategory[] items)
  {
    mSnapshot = Collections.unmodifiableList(Arrays.asList(items));
  }

  protected List<BookmarkCategory> items()
  {
    return mSnapshot;
  }

  public static class Default extends AbstractCategoriesSnapshot
  {
    @NonNull
    private final FilterStrategy mStrategy;

    public Default(@NonNull BookmarkCategory[] items, @NonNull FilterStrategy strategy)
    {
      super(items);
      mStrategy = strategy;
    }

    @Override
    @NonNull
    public final List<BookmarkCategory> items()
    {
      return mStrategy.filter(super.items());
    }
  }

  public static class Owned extends Default
  {
    public Owned(@NonNull BookmarkCategory[] items)
    {
      super(items, new FilterStrategy.Owned());
    }
  }

  public static class Catalog extends Default
  {
    public Catalog(@NonNull BookmarkCategory[] items)
    {
      super(items, new FilterStrategy.Catalog());
    }
  }

  public static class All extends Default {

    public All(@NonNull BookmarkCategory[] items)
    {
      super(items, new FilterStrategy.All());
    }
  }

  private abstract static class FilterStrategy
  {
    @NonNull
    public abstract List<BookmarkCategory> filter(@NonNull List<BookmarkCategory> items);

    public static class All extends FilterStrategy
    {
      @Override
      public List<BookmarkCategory> filter(@NonNull List<BookmarkCategory> items)
      {
        return items;
      }
    }

    private static class Owned extends PredicativeStrategy<Boolean>
    {
      private Owned()
      {
        super(new Predicate.Equals<>(new BookmarkCategory.IsFromCatalog(), false));
      }
    }

    private static class Catalog extends PredicativeStrategy<Boolean>
    {
      private Catalog()
      {
        super(new Predicate.Equals<>(new BookmarkCategory.IsFromCatalog(), true));
      }
    }
  }

  public static class PredicativeStrategy<T> extends FilterStrategy
  {
    @NonNull
    private final Predicate<T, BookmarkCategory> mPredicate;

    private PredicativeStrategy(@NonNull Predicate<T, BookmarkCategory> predicate)
    {
      mPredicate = predicate;
    }

    @Override
    public List<BookmarkCategory> filter(@NonNull List<BookmarkCategory> items)
    {
      List<BookmarkCategory> result = new ArrayList<>();
      for (BookmarkCategory each : items)
      {
        if (mPredicate.apply(each))
        {
          result.add(each);
        }
      }
      return Collections.unmodifiableList(result);
    }
  }
}
