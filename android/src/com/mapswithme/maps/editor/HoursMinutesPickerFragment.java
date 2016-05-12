package com.mapswithme.maps.editor;

import android.annotation.SuppressLint;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.res.ColorStateList;
import android.os.Bundle;
import android.support.annotation.IntRange;
import android.support.annotation.NonNull;
import android.support.design.widget.TabLayout;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;
import android.support.v7.app.AlertDialog;
import android.text.format.DateFormat;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import android.widget.TimePicker;

import com.mapswithme.maps.R;
import com.mapswithme.maps.base.BaseMwmDialogFragment;
import com.mapswithme.maps.editor.data.HoursMinutes;
import com.mapswithme.util.ThemeUtils;

public class HoursMinutesPickerFragment extends BaseMwmDialogFragment
{
  private static final String EXTRA_FROM = "HoursMinutesFrom";
  private static final String EXTRA_TO = "HoursMinutesTo";
  private static final String EXTRA_SELECT_FIRST = "SelectedTab";
  private static final String EXTRA_ID = "Id";

  public static final int TAB_FROM = 0;
  public static final int TAB_TO = 1;

  private HoursMinutes mFrom;
  private HoursMinutes mTo;

  private TimePicker mPicker;
  @IntRange(from = 0, to = 1) private int mSelectedTab;
  private TabLayout mTabs;

  private int mId;
  private Button mOkButton;

  public interface OnPickListener
  {
    void onHoursMinutesPicked(HoursMinutes from, HoursMinutes to, int id);
  }

  public static void pick(Context context, FragmentManager manager, @NonNull HoursMinutes from, @NonNull HoursMinutes to,
                          @IntRange(from = 0, to = 1) int selectedPosition, int id)
  {
    final Bundle args = new Bundle();
    args.putParcelable(EXTRA_FROM, from);
    args.putParcelable(EXTRA_TO, to);
    args.putInt(EXTRA_SELECT_FIRST, selectedPosition);
    args.putInt(EXTRA_ID, id);
    final HoursMinutesPickerFragment fragment =
        (HoursMinutesPickerFragment) Fragment.instantiate(context, HoursMinutesPickerFragment.class.getName(), args);
    fragment.show(manager, null);
  }

  @NonNull
  @Override
  public Dialog onCreateDialog(Bundle savedInstanceState)
  {
    readArgs();
    final View root = createView();
    //noinspection ConstantConditions
    mTabs.getTabAt(mSelectedTab).select();
    final int theme = ThemeUtils.isNightTheme() ? R.style.MwmMain_DialogFragment_TimePicker_Night
                                                : R.style.MwmMain_DialogFragment_TimePicker;
    final AlertDialog dialog = new AlertDialog.Builder(getActivity(), theme)
                                   .setView(root)
                                   .setNegativeButton(android.R.string.cancel, null)
                                   .setPositiveButton(android.R.string.ok, null)
                                   .setCancelable(true)
                                   .create();

    dialog.setOnShowListener(new DialogInterface.OnShowListener()
    {
      @Override
      public void onShow(DialogInterface dialogInterface)
      {
        mOkButton = dialog.getButton(AlertDialog.BUTTON_POSITIVE);
        mOkButton.setOnClickListener(new View.OnClickListener()
        {
          @Override
          public void onClick(View v)
          {
            if (mSelectedTab == TAB_FROM)
            {
              //noinspection ConstantConditions
              mTabs.getTabAt(TAB_TO).select();
              return;
            }

            dismiss();
            if (getParentFragment() instanceof OnPickListener)
              ((OnPickListener) getParentFragment()).onHoursMinutesPicked(mFrom, mTo, mId);
          }
        });
        refreshPicker();
      }
    });

    return dialog;
  }

  private void readArgs()
  {
    final Bundle args = getArguments();
    mFrom = args.getParcelable(EXTRA_FROM);
    mTo = args.getParcelable(EXTRA_TO);
    mSelectedTab = args.getInt(EXTRA_SELECT_FIRST);
    mId = args.getInt(EXTRA_ID);
  }

  private View createView()
  {
    final LayoutInflater inflater = LayoutInflater.from(getActivity());
    @SuppressLint("InflateParams")
    final View root = inflater.inflate(R.layout.fragment_timetable_picker, null);

    mTabs = (TabLayout) root.findViewById(R.id.tabs);
    TextView tabView = (TextView) inflater.inflate(R.layout.tab_timepicker, mTabs, false);
    // TODO @yunik add translations
    tabView.setText("From");
    final ColorStateList textColor = getResources().getColorStateList(ThemeUtils.isNightTheme() ? R.color.tab_text_night
                                                                                                : R.color.tab_text);
    tabView.setTextColor(textColor);
    mTabs.addTab(mTabs.newTab().setCustomView(tabView), true);
    tabView = (TextView) inflater.inflate(R.layout.tab_timepicker, mTabs, false);
    tabView.setText("To");
    tabView.setTextColor(textColor);
    mTabs.addTab(mTabs.newTab().setCustomView(tabView), true);
    mTabs.setOnTabSelectedListener(new TabLayout.OnTabSelectedListener()
    {
      @Override
      public void onTabSelected(TabLayout.Tab tab)
      {
        if (!isInit())
          return;

        saveHoursMinutes();
        mSelectedTab = tab.getPosition();
        refreshPicker();
      }

      @Override
      public void onTabUnselected(TabLayout.Tab tab) {}

      @Override
      public void onTabReselected(TabLayout.Tab tab) {}
    });

    mPicker = (TimePicker) root.findViewById(R.id.picker);
    mPicker.setIs24HourView(DateFormat.is24HourFormat(getActivity()));
    return root;
  }

  private void saveHoursMinutes()
  {
    final HoursMinutes hoursMinutes = new HoursMinutes(mPicker.getCurrentHour(), mPicker.getCurrentMinute());
    if (mSelectedTab == TAB_FROM)
      mFrom = hoursMinutes;
    else
      mTo = hoursMinutes;
  }

  private boolean isInit()
  {
    return mOkButton != null && mPicker != null;
  }

  private void refreshPicker()
  {
    if (!isInit())
      return;

    HoursMinutes hoursMinutes;
    int okBtnRes;
    if (mSelectedTab == TAB_FROM)
    {
      hoursMinutes = mFrom;
      okBtnRes = R.string.whats_new_next_button;
    }
    else
    {
      hoursMinutes = mTo;
      okBtnRes = R.string.ok;
    }
    mPicker.setCurrentMinute((int) hoursMinutes.minutes);
    mPicker.setCurrentHour((int) hoursMinutes.hours);
    mOkButton.setText(okBtnRes);
  }
}
