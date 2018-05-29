package com.mapswithme.maps.bookmarks;

import android.support.v4.app.Fragment;
import android.view.View;
import android.webkit.WebView;

import com.mapswithme.maps.R;
import com.mapswithme.maps.base.BaseToolbarActivity;

public class BookmarksCatalogActivity extends BaseToolbarActivity
{
  @Override
  protected Class<? extends Fragment> getFragmentClass()
  {
    return BookmarksCatalogFragment.class;
  }

  @Override
  public void onBackPressed()
  {
    super.onBackPressed();
    WebView webView = (WebView) findViewById(R.id.webview);
    if (webView != null && webView.canGoBack())
    {
      webView.goBack();
    }
  }
}
