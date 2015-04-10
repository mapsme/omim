package me.maps.mwmwear.fragment;

import android.app.Fragment;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v7.widget.RecyclerView;
import android.support.wearable.view.WearableListView;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import me.maps.mwmwear.R;
import me.maps.mwmwear.WearMwmActivity;

public class SearchFragment extends Fragment implements WearableListView.ClickListener
{
  private WearableListView mListView;
  private SearchAdapter mSearchAdapter;
  private CategoriesAdapter mCategoriesAdapter;
  private boolean mShowCategories = true; // true to show categories, false to show search results

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
  {
    return inflater.inflate(R.layout.fragment_search, container, false);
  }

  @Override
  public void onViewCreated(View view, Bundle savedInstanceState)
  {
    mListView = (WearableListView) view.findViewById(R.id.lv__categories);
    mListView.setAdapter(getCurrentAdapter());
    mListView.setGreedyTouchMode(true);
    mListView.setClickListener(this);
  }

  private RecyclerView.Adapter getCurrentAdapter()
  {
    if (mShowCategories)
      return getCategoriesAdapter();
    else
      return getSearchAdapter();
  }

  private RecyclerView.Adapter getSearchAdapter()
  {
    if (mSearchAdapter == null)
      mSearchAdapter = new SearchAdapter(this);

    return mSearchAdapter;
  }

  private RecyclerView.Adapter getCategoriesAdapter()
  {
    if (mCategoriesAdapter == null)
      mCategoriesAdapter = new CategoriesAdapter(this);

    return mCategoriesAdapter;
  }

  public boolean doShowCategories()
  {
    return mShowCategories;
  }

  @Override
  public void onClick(WearableListView.ViewHolder viewHolder)
  {
    Log.d("Wear", "Category Click");
    if (mShowCategories)
    {
      // TODO get results
      if (getActivity() instanceof WearMwmActivity && viewHolder instanceof CategoriesAdapter.ViewHolder)
      {
        WearMwmActivity host = (WearMwmActivity) getActivity();
        CategoriesAdapter.ViewHolder catHolder = (CategoriesAdapter.ViewHolder) viewHolder;
        host.getWearableManager().makeSearchCategoryRequest(catHolder.getCategoryQuery(host));
      }
    }
    else
    {
      // TODO display arrow to search result
    }
  }

  @Override
  public void onTopEmptyRegionClick() {}
}
