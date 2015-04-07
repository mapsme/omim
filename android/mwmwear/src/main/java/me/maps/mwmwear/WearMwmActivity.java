package me.maps.mwmwear;

import android.app.Activity;
import android.os.Bundle;
import android.support.wearable.view.WatchViewStub;
import android.widget.TextView;

public class WearMwmActivity extends Activity
{
  private TextView mTextView;

  @Override
  protected void onCreate(Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.activity_main);
    final WatchViewStub stub = (WatchViewStub) findViewById(R.id.watch_view_stub);
    stub.setRectLayout(R.layout.rect_activity_main);
    stub.setRoundLayout(R.layout.round_activity_main);
    stub.setOnLayoutInflatedListener(new WatchViewStub.OnLayoutInflatedListener()
    {
      @Override
      public void onLayoutInflated(WatchViewStub stub)
      {
        mTextView = (TextView) stub.findViewById(R.id.text);
        mTextView.setText("Hello world!");
      }
    });
  }
}
