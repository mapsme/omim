package com.mapswithme.maps.bookmarks;

import android.os.Build;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import com.mapswithme.maps.R;
import com.mapswithme.maps.base.BaseMwmFragment;

import java.io.IOException;

public class BookmarksCatalogFragment extends BaseMwmFragment
{
  public static final String EXTRA_BOOKMARKS_CATALOG_URL = "bookmarks_catalog_url";
  public static final int INITIAL_SCALE = 1;
  @NonNull
  private String mCatalogUrl;

  @Override
  public void onCreate(@Nullable Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);
    Bundle args = getArguments();
    if ((args == null || (mCatalogUrl = args.getString(EXTRA_BOOKMARKS_CATALOG_URL)) == null)
        && (mCatalogUrl = getActivity().getIntent()
                                       .getStringExtra(EXTRA_BOOKMARKS_CATALOG_URL)) == null)
    {
      throw new IllegalArgumentException("Catalog url not found in bundle");
    }
  }

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater,
                           @Nullable ViewGroup container,
                           @Nullable Bundle savedInstanceState)
  {
    View root = inflater.inflate(R.layout.bookmarks_catalog_frag, null);
    WebView webView = root.findViewById(R.id.webview);
    initWebView(webView);
    webView.loadUrl(mCatalogUrl);
    return root;
  }

  private void initWebView(WebView webView)
  {
    webView.setWebViewClient(new WebViewBookmarksCatalogClient());
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP)
    {
      webView.getSettings().setMixedContentMode(WebSettings.MIXED_CONTENT_ALWAYS_ALLOW);
    }
    webView.setVerticalScrollBarEnabled(true);
    webView.setScrollBarStyle(WebView.SCROLLBARS_INSIDE_OVERLAY);
    webView.setOverScrollMode(View.OVER_SCROLL_NEVER);
    webView.setLongClickable(true);
    webView.setInitialScale(INITIAL_SCALE);

    final WebSettings webSettings = webView.getSettings();

    webSettings.setCacheMode(WebSettings.LOAD_NO_CACHE);
    webSettings.setSupportZoom(true);
    webSettings.setBuiltInZoomControls(true);
    webSettings.setUseWideViewPort(true);
    webSettings.setLoadWithOverviewMode(true);
    webSettings.setDisplayZoomControls(false);
    webSettings.setJavaScriptEnabled(true);
  }

  private static class WebViewBookmarksCatalogClient extends WebViewClient
  {
    @Override
    public boolean shouldOverrideUrlLoading(WebView view, String url)
    {
      try
      {
        return requestArchive(view, url);
      }
      catch (IOException e)
      {
        return super.shouldOverrideUrlLoading(view, url);
      }
    }

    private boolean requestArchive(WebView view, String url) throws IOException
    {
      BookmarksDownloadManager dm = BookmarksDownloadManager.from(view.getContext());
      dm.enqueueRequest(url);
      return true;
    }
  }
}
