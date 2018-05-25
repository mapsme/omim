package com.mapswithme.maps.bookmarks;

import android.app.DownloadManager;
import android.content.Context;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.text.TextUtils;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import com.mapswithme.maps.R;
import com.mapswithme.maps.base.BaseMwmFragment;
import com.mapswithme.maps.bookmarks.data.BookmarkManager;

import java.util.Set;

import static android.content.Context.DOWNLOAD_SERVICE;

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
    webView.setWebViewClient(new WebViewBookmarksCatalogClient(getContext()));
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

  /*FIXME*/
  private static class WebViewBookmarksCatalogClient extends WebViewClient
  {
    public static final String DOWNLOAD_ARCHIVE_SCHEME = "https";
    public static final String QUERY_PARAM_ID_KEY = "id";
    public static final String QUERY_PARAM_NAME_KEY = "name";

    @NonNull
    private final Context mContext;

    private WebViewBookmarksCatalogClient(@NonNull Context context)
    {
      mContext = context.getApplicationContext();
    }

    @Override
    public boolean shouldOverrideUrlLoading(WebView view, String url)
    {
      Uri srcUri = null;
      DownloadManager downloadManager = (DownloadManager)mContext.getSystemService(DOWNLOAD_SERVICE);
      if (downloadManager != null){
        Pair<Uri, Uri> uriPair = onPrepareUri(url);
        srcUri = uriPair.first;
        Uri dstUri = uriPair.second;

        String title = getTitle(srcUri);
        DownloadManager.Request request = new DownloadManager
            .Request(dstUri)
            .setNotificationVisibility(DownloadManager.Request.VISIBILITY_VISIBLE_NOTIFY_COMPLETED)
            .setTitle(title)
            .setDestinationInExternalFilesDir(mContext, null, dstUri.getLastPathSegment());
        downloadManager.enqueue(request);
      }
      return !DOWNLOAD_ARCHIVE_SCHEME.equals(srcUri == null ? null : srcUri.getScheme());
    }

    private String getTitle(Uri srcUri)
    {
      String title;
      return TextUtils.isEmpty((title = srcUri.getQueryParameter(QUERY_PARAM_NAME_KEY)))
             ? srcUri.getQueryParameter(QUERY_PARAM_ID_KEY)
             : title;
    }

    private Pair<Uri, Uri> onPrepareUri(String url)
    {
      Uri srcUri = Uri.parse(url);
      String fileId = srcUri.getQueryParameter(QUERY_PARAM_ID_KEY);
      if (TextUtils.isEmpty(fileId)){
        throw new IllegalArgumentException("query param id not found");
      }
      String downloadUrl = BookmarkManager.INSTANCE.getCatalogDownloadUrl(fileId);
      Uri.Builder builder = Uri.parse(downloadUrl).buildUpon();

      Set<String> paramNames = srcUri.getQueryParameterNames();
      for (String each : paramNames){
        builder.appendQueryParameter(each, srcUri.getQueryParameter(each));
      }
      Uri dstUri = builder.build();
      return new Pair<Uri, Uri>(srcUri, dstUri);
    }
  }
}
