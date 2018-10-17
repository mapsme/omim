package com.mapswithme.maps.search;

import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import com.mapswithme.maps.R;
import com.mapswithme.maps.base.BaseMwmRecyclerFragment;

public class SearchCategoriesFragment extends BaseMwmRecyclerFragment<CategoriesAdapter>
                                   implements CategoriesAdapter.OnCategorySelectedListener
{
  @NonNull
  @Override
  protected CategoriesAdapter createAdapter()
  {
    return new CategoriesAdapter(this);
  }

  @Override
  protected int getLayoutRes()
  {
    return R.layout.fragment_search_categories;
  }


  protected void safeOnActivityCreated(@Nullable Bundle savedInstanceState)
  {
    ((SearchFragment) getParentFragment()).setRecyclerScrollListener(getRecyclerView());
  }

  @Override
  public void onSearchCategorySelected(String category)
  {
    if (!passCategory(getParentFragment(), category))
      passCategory(getActivity(), category);
  }

  @Override
  public void onPromoCategorySelected(@NonNull PromoCategory promo)
  {
    PromoCategoryProcessor processor = promo.createProcessor(getContext().getApplicationContext());
    processor.process();
  }

  private static boolean passCategory(Object listener, String category)
  {
    if (!(listener instanceof CategoriesAdapter.OnCategorySelectedListener))
      return false;

    ((CategoriesAdapter.OnCategorySelectedListener)listener).onSearchCategorySelected(category);
    return true;
  }
}
