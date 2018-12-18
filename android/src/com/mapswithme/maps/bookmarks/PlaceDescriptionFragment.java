package com.mapswithme.maps.bookmarks;

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
import com.mapswithme.util.Utils;
import com.mapswithme.util.statistics.Statistics;

import java.util.Objects;

public class PlaceDescriptionFragment extends BaseMwmFragment
{
  private static final String WIDTH_FIT_CENTER = "width=\"100%\" ";
  private static final String SRC_TAG = "src";
  public static final String EXTRA_DESCRIPTION = "description";

  @SuppressWarnings("NullableProblems")
  @NonNull
  private String mDescription;

  @Override
  public void onCreate(@Nullable Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);
    mDescription = Objects.requireNonNull(getArguments().getString(EXTRA_DESCRIPTION));
  }

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container,
                           @Nullable Bundle savedInstanceState)
  {
    View root = inflater.inflate(R.layout.fragment_place_description, container, false);
    WebView webView = root.findViewById(R.id.webview);
    String preparedData = makeHtmlImagesFitCenter(mDescription);
    webView.setVerticalScrollBarEnabled(true);
    webView.setWebViewClient(new PlaceDescriptionClient());
    webView.loadDataWithBaseURL(null, preparedData, Utils.TEXT_HTML, Utils.UTF_8, null);
    return root;
  }

  @NonNull
  private static String makeHtmlImagesFitCenter(@NonNull String content)
  {
    String stringToAdd = WIDTH_FIT_CENTER;
    StringBuilder builder = new StringBuilder(content);

    int i = 0;
    int cont = 0;
    while (i != -1)
    {
      i = content.indexOf(SRC_TAG, i + 1);
      if (i != -1)
        builder.insert(i + (cont * stringToAdd.length()), stringToAdd);
      ++cont;
    }
    return builder.toString();
  }

  private static class PlaceDescriptionClient extends WebViewClient
  {
    @Override
    public boolean shouldOverrideUrlLoading(WebView view, String url)
    {
      Statistics.ParameterBuilder params = new Statistics.ParameterBuilder().add(Statistics
                                                                                     .EventParam
                                                                                     .URL, url);
      Statistics.INSTANCE.trackEvent(Statistics.EventName.PLACEPAGE_DESCRIPTION_OUTBOUND_CLICK,
                                     params);
      return super.shouldOverrideUrlLoading(view, url);
    }
  }
}
