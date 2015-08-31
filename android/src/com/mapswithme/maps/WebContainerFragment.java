package com.mapswithme.maps;

import android.annotation.SuppressLint;
import android.content.Intent;
import android.net.MailTo;
import android.net.Uri;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import com.mapswithme.maps.base.BaseMwmFragment;
import com.mapswithme.util.UiUtils;

public class WebContainerFragment extends BaseMwmFragment
{
  private WebView mWebView;
  private View mProgress;


  @SuppressLint("SetJavaScriptEnabled")
  private void initWebView()
  {
    String url = getArguments().getString(WebContainerActivity.EXTRA_URL);

    mWebView.setWebViewClient(new WebViewClient()
    {
      @Override
      public void onPageFinished(WebView view, String url)
      {
        UiUtils.show(mWebView);
        UiUtils.hide(mProgress);
      }

      @Override
      public boolean shouldOverrideUrlLoading(WebView v, String url)
      {
        if (MailTo.isMailTo(url))
        {
          MailTo parser = MailTo.parse(url);
          startActivity(new Intent(Intent.ACTION_SEND)
                            .putExtra(Intent.EXTRA_EMAIL, new String[] { parser.getTo() })
                            .putExtra(Intent.EXTRA_TEXT, parser.getBody())
                            .putExtra(Intent.EXTRA_SUBJECT, parser.getSubject())
                            .putExtra(Intent.EXTRA_CC, parser.getCc())
                            .setType("message/rfc822"));
          v.reload();
          return true;
        }

        startActivity(new Intent(Intent.ACTION_VIEW)
                          .setData(Uri.parse(url)));
        return true;
      }
    });

    mWebView.getSettings().setJavaScriptEnabled(true);
    mWebView.getSettings().setDefaultTextEncodingName("utf-8");
    mWebView.loadUrl(url);
  }

  @Override
  public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState)
  {
    View res = inflater.inflate(R.layout.webview_container, container, false);

    mWebView = (WebView) res.findViewById(R.id.webview);
    mProgress = res.findViewById(R.id.progress);
    initWebView();

    return res;
  }
}
