package com.mapswithme.maps.bookmarks;

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
import com.mapswithme.maps.auth.BaseWebViewMwmFragment;

import java.io.IOException;

public class BookmarksCatalogFragment extends BaseWebViewMwmFragment
{
  public static final String EXTRA_BOOKMARKS_CATALOG_URL = "bookmarks_catalog_url";
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
    View root = inflater.inflate(R.layout.bookmarks_catalog_frag, container, false);
    WebView webView = root.findViewById(getWebViewResId());
    initWebView(webView);
    webView.loadUrl(mCatalogUrl);
    return root;
  }

  private void initWebView(WebView webView)
  {
    webView.setWebViewClient(new WebViewBookmarksCatalogClient());
    final WebSettings webSettings = webView.getSettings();
    webSettings.setJavaScriptEnabled(true);
  }

  @Override
  public boolean onBackPressed()
  {
    return super.onBackPressed();
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

    private boolean requestArchive(@NonNull WebView view, @NonNull String url) throws IOException
    {
/*      BookmarksDownloadManager dm = BookmarksDownloadManager.from(view.getContext());
      dm.enqueueRequest(url);*/
      return true;
    }
  }
}
