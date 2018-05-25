package com.mapswithme.maps.bookmarks;

import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import com.mapswithme.maps.R;
import com.mapswithme.maps.bookmarks.data.BookmarkManager;
import com.mapswithme.util.UiUtils;

public class CachedBookmarksFragment extends BaseBookmarkCategoriesFragment implements
                                                                            BookmarkManager.BookmarksCatalogListener

{
  @NonNull
  private ViewGroup mEmptyViewContainer;
  @NonNull
  private View mPayloadContainer;
  @NonNull
  private View mProgressContainer;

  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
  {
    View root = super.onCreateView(inflater, container, savedInstanceState);
    mProgressContainer = root.findViewById(R.id.placeholder_loading);
    mEmptyViewContainer = root.findViewById(R.id.placeholder_container);
    mPayloadContainer = root.findViewById(R.id.cached_bookmarks_payload_container);
    View downloadBtn = mEmptyViewContainer.findViewById(R.id.download_routers_btn);
    downloadBtn.setOnClickListener(new DownloadRoutesClickListener());
    View closeHeaderBtn = root.findViewById(R.id.header_close);
    closeHeaderBtn.setOnClickListener(new CloseHeaderClickListener());

    return root;
  }

  @Override
  protected int getLayoutRes()
  {
    return R.layout.cached_bookmarks_frag;
  }

  @Override
  protected RecyclerView.Adapter createAdapter()
  {
    return new CatalogBookmarkCategoriesAdapter(getContext());
  }

  @Override
  public void onFooterClick()
  {
    openBookmarksCatalogScreen();
  }

  @Override
  protected void updateLoadingPlaceholder()
  {
    super.updateLoadingPlaceholder();
    boolean showLoadingPlaceholder = BookmarkManager.INSTANCE.isAsyncBookmarksLoadingInProgress();
    if (showLoadingPlaceholder){
      mProgressContainer.setVisibility(View.VISIBLE);
      mPayloadContainer.setVisibility(View.GONE);
      mEmptyViewContainer.setVisibility(View.GONE);
    } else {
      mProgressContainer.setVisibility(View.GONE);
      boolean isEmptyAdapter = getAdapter().getItemCount() == 0;
      mEmptyViewContainer.setVisibility(isEmptyAdapter ? View.VISIBLE : View.GONE);
      mPayloadContainer.setVisibility(isEmptyAdapter ? View.GONE : View.VISIBLE);
    }
  }

  @Override
  public void onStart()
  {
    super.onStart();
    BookmarkManager.INSTANCE.addCatalogListener(this);
  }

  @Override
  public void onStop()
  {
    super.onStop();
    BookmarkManager.INSTANCE.removeCatalogListener(this);
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

  @Override
  public void onImportStarted(@NonNull String serverId)
  {
    mProgressContainer.setVisibility(View.VISIBLE);
    mEmptyViewContainer.setVisibility(View.GONE);
    mPayloadContainer.setVisibility(View.GONE);
  }

  @Override
  public void onImportFinished(@NonNull String serverId, boolean successful)
  {
    if (successful){
      mPayloadContainer.setVisibility(View.VISIBLE);
      mProgressContainer.setVisibility(View.GONE);
      mEmptyViewContainer.setVisibility(View.GONE);
      getAdapter().notifyDataSetChanged();
    } else {
      boolean isEmptyAdapter = getAdapter().getItemCount() == 0;
      mProgressContainer.setVisibility(View.GONE);
      UiUtils.showIf(isEmptyAdapter, mEmptyViewContainer);
      UiUtils.showIf(!isEmptyAdapter,mPayloadContainer);
    }
  }

  private class CloseHeaderClickListener implements View.OnClickListener
  {
    @Override
    public void onClick(View v)
    {
      View header = mPayloadContainer.findViewById(R.id.header);
      header.setVisibility(View.GONE);
    }
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
