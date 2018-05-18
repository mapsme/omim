package com.mapswithme.maps.bookmarks;

import android.app.DownloadManager;
import android.content.Context;
import android.net.Uri;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import com.mapswithme.maps.R;
import com.mapswithme.maps.base.BaseMwmFragment;

import static android.content.Context.DOWNLOAD_SERVICE;

public class BookmarksCatalogFragment extends BaseMwmFragment
{

  public static final String EXTRA_BOOKMARKS_CATALOG_URL = "bookmarks_catalog_url";
  private static final String STUB_DATA = "<a href=\"mapsme://dlink.maps.me/catalogue?id=a9e4e048-f864-4209-b64f-1db1ebdfb16b&amp;name=bundle+number+one\">\n" +
                                          "    <button>Click me</button>\n" +
                                          "</a>";
  @NonNull
  private String mCatalogUrl;

  public BookmarksCatalogFragment()
  {
  }

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
    webView.setWebViewClient(new WebViewBookmarksCatalogClient(getContext()));
    String baseUri = "https://" + "e.mail.ru";
    webView.loadDataWithBaseURL(baseUri, STUB_DATA, "text/html", "utf-8", null);
    return root;
  }

  private static class WebViewBookmarksCatalogClient extends WebViewClient
  {
    @NonNull
    private final Context mContext;

    private WebViewBookmarksCatalogClient(@NonNull Context context)
    {
      mContext = context.getApplicationContext();
    }

    @Override
    public boolean shouldOverrideUrlLoading(WebView view, String url)
    {
      DownloadManager downloadManager = (DownloadManager)mContext.getSystemService(DOWNLOAD_SERVICE);
      if (downloadManager != null){
        Uri uri = onPrepareUri(url);
        DownloadManager.Request request = new DownloadManager.Request(uri);
        request.setDestinationInExternalFilesDir(mContext, null, uri.getLastPathSegment());
        downloadManager.enqueue(request);
      }
      return super.shouldOverrideUrlLoading(view, url);
    }

    private Uri onPrepareUri(String url)
    {
      return Uri.parse("https://cdn.empireonline.com/jpg/70/0/0/640/480/aspectfit/0/0/0/0/0/0/c/articles/5a8373cc59a3c7762a368381/Jason-Statham.jpg");
    }
  }
}
