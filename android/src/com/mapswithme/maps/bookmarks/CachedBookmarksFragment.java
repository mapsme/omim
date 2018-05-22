package com.mapswithme.maps.bookmarks;

import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import com.mapswithme.maps.R;
import com.mapswithme.maps.base.BaseMwmFragment;
import com.mapswithme.maps.widget.recycler.ItemDecoratorFactory;
import com.mapswithme.maps.widget.recycler.RecyclerLongClickListener;

public class CachedBookmarksFragment extends BaseMwmFragment implements RecyclerLongClickListener,
                                                                        CategoryListCallback
{
  /*FIXME*/
  private BookmarkCategoriesAdapter mAdapter;
  /*FIXME*/
  @NonNull
  private ViewGroup mEmptyViewContainer;
  private View mPayloadContainer;

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater,
                           @Nullable ViewGroup container,
                           @Nullable Bundle savedInstanceState)
  {
    View root = inflater.inflate(R.layout.cached_bookmarks_frag, null);
    initEmptyView(root);
    initRecycler(root);
    mPayloadContainer = root.findViewById(R.id.cached_bookmarks_payload_container);
    mPayloadContainer.setVisibility(View.GONE);
    mEmptyViewContainer.setVisibility(View.VISIBLE);
    return root;
  }

  private void initEmptyView(View root)
  {
    mEmptyViewContainer = root.findViewById(R.id.placeholder_container);
    View downloadRoutesBtn = mEmptyViewContainer.findViewById(R.id.download_routers_btn);
    downloadRoutesBtn.setOnClickListener(new DownloadRoutesClickListener());
    mEmptyViewContainer.setVisibility(View.VISIBLE);
  }

  private void initRecycler(View root)
  {
    RecyclerView recycler = root.findViewById(R.id.recycler);
    LinearLayoutManager layoutManager = new LinearLayoutManager(getContext(),
                                                                LinearLayoutManager.VERTICAL,
                                                                false);
    recycler.setLayoutManager(layoutManager);
    recycler.setNestedScrollingEnabled(false);
    recycler.addItemDecoration(ItemDecoratorFactory.createVerticalDefaultDecorator(getContext()));
    mAdapter = new CatalogBookmarkCategoriesAdapter(getContext());
    mAdapter.setOnLongClickListener(this);
    mAdapter.setCategoryListCallback(this);
    recycler.setAdapter(mAdapter);
  }

  @Override
  public void onLongItemClick(View v, int position)
  {

  }

  @Override
  public void onFooterClick()
  {
    openBookmarksCatalogScreen();

  }

  private void openBookmarksCatalogScreen()
  {
    Intent intent = new Intent(getActivity(),
                               BookmarksCatalogActivity.class).putExtra(
                                   BookmarksCatalogFragment.EXTRA_BOOKMARKS_CATALOG_URL,
                                   getCatalogUrl());
    getActivity().startActivity(intent);
  }

  /*FIXME*/
  @NonNull
  private String getCatalogUrl()
  {
    return "https://bookcat.demo.mapsme1.devmail.ru/simplefront/all";
  }

  @Override
  public void onMoreOperationClick(int position)
  {

  }

  private class DownloadRoutesClickListener implements View.OnClickListener
  {
    @Override
    public void onClick(View v)
    {
      openBookmarksCatalogScreen();
    }
  }
}
