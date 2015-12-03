package com.mapswithme.maps.widget.placepage;

import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import com.mapswithme.maps.R;
import com.mapswithme.maps.base.BaseMwmToolbarFragment;
import com.mapswithme.maps.base.OnBackPressListener;
import com.mapswithme.maps.editor.OpeningHours;
import com.mapswithme.maps.editor.data.Timetable;
import com.mapswithme.util.UiUtils;
import com.mapswithme.util.Utils;

public class TimetableFragment extends BaseMwmToolbarFragment
                            implements View.OnClickListener,
                                       OnBackPressListener
{
  interface TimetableProvider
  {
    Timetable[] getTimetables();
  }

  public static final String EXTRA_TIME = "Time";

  private boolean mIsAdvancedMode;

  private TextView mSwitchMode;

  private TimetableSimpleFragment mSimpleModeFragment;
  private TimetableAdvancedFragment mAdvancedModeFragment;

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
  {
    return inflater.inflate(R.layout.fragment_timetable, container, false);
  }

  @Override
  public void onViewCreated(View view, @Nullable Bundle savedInstanceState)
  {
    super.onViewCreated(view, savedInstanceState);

    initViews(view);
    simpleMode();

    final Bundle args = getArguments();
    if (args != null && args.containsKey(EXTRA_TIME))
      mSimpleModeFragment.setTimetables(OpeningHours.nativeTimetablesFromString(args.getString(EXTRA_TIME)));
  }

  private void initViews(View root)
  {
    mToolbarController.setTitle(R.string.editor_time_title);
    mToolbarController.findViewById(R.id.iv__submit).setOnClickListener(this);
    mSwitchMode = (TextView) root.findViewById(R.id.tv__mode_switch);
    mSwitchMode.setOnClickListener(this);
  }

  @Override
  public void onClick(View v)
  {
    switch (v.getId())
    {
    case R.id.tv__mode_switch:
      switchMode();
      break;
    case R.id.iv__submit:
      saveTimetable();
    }
  }

  @Override
  public boolean onBackPressed()
  {
    return false;
  }

  private void switchMode()
  {
    if (mIsAdvancedMode)
      simpleMode();
    else
      advancedMode();
  }

  private void simpleMode()
  {
    mIsAdvancedMode = false;
    mSwitchMode.setText(R.string.editor_time_advanced);
    final Timetable[] filledTts = getFilledTimetables(mAdvancedModeFragment, mAdvancedModeFragment);
    mSimpleModeFragment = (TimetableSimpleFragment) attachFragment(mSimpleModeFragment, TimetableSimpleFragment.class.getName());
    mSimpleModeFragment.setTimetables(filledTts);
  }

  private void advancedMode()
  {
    mIsAdvancedMode = true;
    mSwitchMode.setText(R.string.editor_time_simple);
    final Timetable[] filledTts = getFilledTimetables(mSimpleModeFragment, mSimpleModeFragment);
    mAdvancedModeFragment = (TimetableAdvancedFragment) attachFragment(mAdvancedModeFragment, TimetableAdvancedFragment.class.getName());
    mAdvancedModeFragment.setTimetables(filledTts);
  }

  private boolean hasFilledTimetables(Fragment fragment)
  {
    return fragment != null && fragment.isAdded();
  }

  private Timetable[] getFilledTimetables(Fragment fragment, TimetableProvider provider)
  {
    if (!hasFilledTimetables(fragment))
      return null;

    final Timetable[] tts = provider.getTimetables();
    if (tts == null)
    {
      // FIXME @yunikkk add correct text
      UiUtils.showAlertDialog(getActivity(), R.string.dialog_routing_system_error);
      return null;
    }

    return tts;
  }

  private Fragment attachFragment(Fragment current, String className)
  {
    Fragment fragment = current == null ? Fragment.instantiate(getActivity(), className)
                                        : current;
    getChildFragmentManager().beginTransaction().replace(R.id.fragment_container, fragment).commit();
    return fragment;
  }

  private void saveTimetable()
  {
    if (mIsAdvancedMode)
      mAdvancedModeFragment.getTimetables();
    else
      mSimpleModeFragment.getTimetables();

    final Timetable[] tts = mIsAdvancedMode ? mAdvancedModeFragment.getTimetables() : mSimpleModeFragment.getTimetables();
    // TODO @yunikkk or @deathbaba save tts to the core

    Utils.navigateToParent(getActivity());
  }
}
