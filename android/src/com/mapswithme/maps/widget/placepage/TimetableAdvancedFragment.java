package com.mapswithme.maps.widget.placepage;

import android.os.Bundle;
import android.support.annotation.Nullable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.ImageView;

import com.mapswithme.maps.R;
import com.mapswithme.maps.base.BaseMwmFragment;
import com.mapswithme.maps.editor.OpeningHours;
import com.mapswithme.maps.editor.data.Timetable;
import com.mapswithme.util.UiUtils;

public class TimetableAdvancedFragment extends BaseMwmFragment
                                    implements View.OnClickListener,
                                               TimetableFragment.TimetableProvider
{
  private boolean mIsExampleShown;
  private EditText mInput;
  private View mExample;
  private ImageView mIndicator;
  private Timetable[] mInitTts;

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
  {
    return inflater.inflate(R.layout.fragment_timetable_advanced, container, false);
  }

  @Override
  public void onViewCreated(View view, @Nullable Bundle savedInstanceState)
  {
    super.onViewCreated(view, savedInstanceState);
    initViews(view);
    refreshTts();
    hideExample();
  }

  @Override
  public void onViewStateRestored(@Nullable Bundle savedInstanceState)
  {
    super.onViewStateRestored(savedInstanceState);
    refreshTts();
  }

  private void initViews(View view)
  {
    view.findViewById(R.id.examples).setOnClickListener(this);
    mInput = (EditText) view.findViewById(R.id.et__timetable);
    mExample = view.findViewById(R.id.tv__examples);
    mIndicator = (ImageView) view.findViewById(R.id.iv__indicator);
  }

  private void showExample()
  {
    mIsExampleShown = true;
    UiUtils.show(mExample);
    // TODO yunikkk animate indicator
    mIndicator.setImageResource(R.drawable.ic_expand_less);
  }

  private void hideExample()
  {
    mIsExampleShown = false;
    UiUtils.hide(mExample);
    mIndicator.setImageResource(R.drawable.ic_expand_more);
  }

  @Override
  public void onClick(View v)
  {
    switch (v.getId())
    {
    case R.id.examples:
      if (mIsExampleShown)
        hideExample();
      else
        showExample();
    }
  }

  @Override
  public Timetable[] getTimetables()
  {
    if (mInput.length() == 0)
      return OpeningHours.nativeGetDefaultTimetables();
    return OpeningHours.nativeTimetablesFromString(mInput.getText().toString());
  }

  public void setTimetables(Timetable[] tts)
  {
    mInitTts = tts;
    refreshTts();
  }

  private void refreshTts()
  {
    if (mInput != null && mInitTts != null)
      mInput.setText(OpeningHours.nativeTimetablesToString(mInitTts));
  }
}
