package com.mapswithme.maps.widget.placepage;

import android.support.annotation.IdRes;
import android.support.v4.app.Fragment;
import android.support.v7.widget.RecyclerView;
import android.util.Log;
import android.util.SparseArray;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.Switch;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import com.mapswithme.maps.R;
import com.mapswithme.maps.editor.OpeningHours;
import com.mapswithme.maps.editor.data.HoursMinutes;
import com.mapswithme.maps.editor.data.Timespan;
import com.mapswithme.maps.editor.data.Timetable;
import com.mapswithme.util.UiUtils;

public class SimpleTimetableAdapter extends RecyclerView.Adapter<SimpleTimetableAdapter.BaseTimetableViewHolder>
                                 implements HoursMinutesPickerFragment.OnPickListener,
                                            TimetableFragment.TimetableProvider
{
  private static final int TYPE_TIMETABLE = 0;
  private static final int TYPE_ADD_TIMETABLE = 1;

  private final Fragment mFragment;

  private List<Timetable> mItems = new ArrayList<>();
  private Timetable mComplementItem;
  private int mPickingPosition;

  SimpleTimetableAdapter(Fragment fragment)
  {
    mFragment = fragment;
    mItems = new ArrayList<>(Arrays.asList(OpeningHours.nativeGetDefaultTimetables()));
  }

  public void setTimetables(Timetable[] tts)
  {
    mItems = new ArrayList<>(Arrays.asList(tts));
    notifyDataSetChanged();
  }

  @Override
  public Timetable[] getTimetables()
  {
    return mItems.toArray(new Timetable[mItems.size()]);
  }

  @Override
  public BaseTimetableViewHolder onCreateViewHolder(ViewGroup parent, int viewType)
  {
    final LayoutInflater inflater = LayoutInflater.from(parent.getContext());
    return viewType == TYPE_TIMETABLE ? new TimetableViewHolder(inflater.inflate(R.layout.item_timetable, parent, false))
                                      : new AddTimetableViewHolder(inflater.inflate(R.layout.item_timetable_add, parent, false));
  }

  @Override
  public void onBindViewHolder(BaseTimetableViewHolder holder, int position)
  {
    holder.onBind();
  }

  @Override
  public int getItemCount()
  {
    return mItems.size() + 1;
  }

  @Override
  public int getItemViewType(int position)
  {
    return position == getItemCount() - 1 ? TYPE_ADD_TIMETABLE
                                          : TYPE_TIMETABLE;
  }

  protected void addTimetable()
  {
    mItems.add(OpeningHours.nativeGetComplementTimetable(mItems.toArray(new Timetable[mItems.size()])));
    printItems();
    notifyItemInserted(mItems.size() - 1);
    refreshComplement();
  }

  protected void removeTimetable(int position)
  {
    mItems.remove(position);
    printItems();
    notifyItemRemoved(position);
    refreshComplement();
  }

  protected void refreshComplement()
  {
    mComplementItem = OpeningHours.nativeGetComplementTimetable(mItems.toArray(new Timetable[mItems.size()]));
    notifyItemChanged(getItemCount() - 1);
  }

  protected void pickTime(int position, int tab)
  {
    final Timetable data = mItems.get(position);
    mPickingPosition = position;
    HoursMinutesPickerFragment.pick(mFragment.getActivity(), mFragment.getChildFragmentManager(),
                                    data.mWorkingTimespan.mStart,
                                    data.mWorkingTimespan.mEnd,
                                    tab);
  }

  @Override
  public void onHoursMinutesPicked(HoursMinutes from, HoursMinutes to)
  {
    mItems.set(mPickingPosition, OpeningHours.nativeSetOpeningTime(mItems.get(mPickingPosition), new Timespan(from, to)));
    printItems();
    notifyItemChanged(mPickingPosition);
  }

  protected void addWorkingDay(int day, int position)
  {
    final Timetable tts[] = mItems.toArray(new Timetable[mItems.size()]);
    mItems = new ArrayList<>(Arrays.asList(OpeningHours.nativeAddWorkingDay(tts, position, day)));
    refreshComplement();
    printItems();
    notifyDataSetChanged();
  }

  protected void removeWorkingDay(int day, int position)
  {
    final Timetable tts[] = mItems.toArray(new Timetable[mItems.size()]);
    mItems = new ArrayList<>(Arrays.asList(OpeningHours.nativeRemoveWorkingDay(tts, position, day)));
    refreshComplement();
    printItems();
    notifyDataSetChanged();
  }

  // FIXME remove
  private void printItems()
  {
    Log.d("TEST", "Items : \n");
    for (Timetable tt : mItems)
      Log.d("TEST", tt.toString() + "\n");
  }

  private void setFullday(int position, boolean fullday)
  {
    mItems.set(position, OpeningHours.nativeSetIsFullday(mItems.get(position), fullday));
    notifyItemChanged(position);
  }

  public abstract class BaseTimetableViewHolder extends RecyclerView.ViewHolder
  {
    public BaseTimetableViewHolder(View itemView)
    {
      super(itemView);
    }

    abstract void onBind();
  }

  public class TimetableViewHolder extends BaseTimetableViewHolder implements View.OnClickListener, CompoundButton.OnCheckedChangeListener
  {
    TextView day1Text;
    TextView day2Text;
    TextView day3Text;
    TextView day4Text;
    TextView day5Text;
    TextView day6Text;
    TextView day7Text;
    SparseArray<CheckBox> days = new SparseArray<>(7);
    View allday;
    Switch swAllday;
    View schedule;
    View openClose;
    View open;
    View close;
    TextView tvOpen;
    TextView tvClose;
    View addClosed;
    View deleteTimetable;

    public TimetableViewHolder(View itemView)
    {
      super(itemView);
      addDay(R.id.chb__day_1, 1);
      addDay(R.id.chb__day_2, 2);
      addDay(R.id.chb__day_3, 3);
      addDay(R.id.chb__day_4, 4);
      addDay(R.id.chb__day_5, 5);
      addDay(R.id.chb__day_6, 6);
      addDay(R.id.chb__day_7, 7);
      fillDaysTexts();
      final int[] daysIds = new int[]{R.id.day1, R.id.day2, R.id.day3, R.id.day4, R.id.day5, R.id.day6, R.id.day7};
      for (int i = 0; i < 7; i++)
      {
        final int finalI = i;
        itemView.findViewById(daysIds[i]).setOnClickListener(new View.OnClickListener()
        {
          @Override
          public void onClick(View v)
          {
            days.get(finalI + 1).toggle();
          }
        });
      }

      allday = itemView.findViewById(R.id.allday);
      allday.setOnClickListener(this);
      swAllday = (Switch) allday.findViewById(R.id.sw__allday);
      schedule = itemView.findViewById(R.id.schedule);
      openClose = schedule.findViewById(R.id.time_open_close);
      open = openClose.findViewById(R.id.time_open);
      open.setOnClickListener(this);
      close = openClose.findViewById(R.id.time_close);
      close.setOnClickListener(this);
      tvOpen = (TextView) open.findViewById(R.id.tv__time_open);
      tvClose = (TextView) close.findViewById(R.id.tv__time_close);
      addClosed = schedule.findViewById(R.id.tv__add_closed);
      addClosed.setOnClickListener(this);
      deleteTimetable = itemView.findViewById(R.id.tv__remove_timetable);
      deleteTimetable.setOnClickListener(this);
    }

    @Override
    void onBind()
    {
      final int position = getAdapterPosition();
      final Timetable data = mItems.get(position);
      UiUtils.showIf(position > 0, deleteTimetable);
      tvOpen.setText(workingTime(data.mWorkingTimespan.mStart));
      tvClose.setText(workingTime(data.mWorkingTimespan.mEnd));
      showDays(data.mWeekdays);
      showSchedule(!data.mIsFullday);
    }

    private String workingTime(HoursMinutes hoursMinutes)
    {
      // TODO @yunik add proper translated strings
      return String.format("%02d", Long.valueOf(hoursMinutes.mHours))
                 + ":"
                 + String.format("%02d", Long.valueOf(hoursMinutes.mMinutes));
    }

    @Override
    public void onClick(View v)
    {
      switch (v.getId())
      {
      case R.id.time_open:
        pickTime(getAdapterPosition(), HoursMinutesPickerFragment.TAB_FROM);
        break;
      case R.id.time_close:
        pickTime(getAdapterPosition(), HoursMinutesPickerFragment.TAB_TO);
        break;
      case R.id.tv__remove_timetable:
        removeTimetable(getAdapterPosition());
        break;
      case R.id.closed:
        // TODO @yunik
        break;
      case R.id.allday:
        swAllday.toggle();
        break;
      }
    }

    @Override
    public void onCheckedChanged(CompoundButton buttonView, boolean isChecked)
    {
      switch (buttonView.getId())
      {
      case R.id.sw__allday:
        setFullday(getAdapterPosition(), isChecked);
        break;
      case R.id.chb__day_1:
        switchWorkingDay(1);
        break;
      case R.id.chb__day_2:
        switchWorkingDay(2);
        break;
      case R.id.chb__day_3:
        switchWorkingDay(3);
        break;
      case R.id.chb__day_4:
        switchWorkingDay(4);
        break;
      case R.id.chb__day_5:
        switchWorkingDay(5);
        break;
      case R.id.chb__day_6:
        switchWorkingDay(6);
        break;
      case R.id.chb__day_7:
        switchWorkingDay(7);
        break;
      }
    }

    void showSchedule(boolean show)
    {
      UiUtils.showIf(show, schedule);
      checkWithoutCallback(swAllday, !show);
    }

    void showDays(int[] weekdays)
    {
      for (int i = 1; i <= 7; i++)
        checkWithoutCallback(days.get(i), false);

      for (int checked : weekdays)
        checkWithoutCallback(days.get(checked), true);
    }

    private void addDay(@IdRes int res, final int dayIndex)
    {
      days.put(dayIndex, (CheckBox) itemView.findViewById(res));
    }

    private void switchWorkingDay(int day)
    {
      final CheckBox checkBox = days.get(day);
      if (checkBox.isChecked())
        addWorkingDay(day, getAdapterPosition());
      else
        removeWorkingDay(day, getAdapterPosition());
    }

    private void fillDaysTexts()
    {
      day1Text = (TextView) itemView.findViewById(R.id.tv__day_1);
      day2Text = (TextView) itemView.findViewById(R.id.tv__day_2);
      day3Text = (TextView) itemView.findViewById(R.id.tv__day_3);
      day4Text = (TextView) itemView.findViewById(R.id.tv__day_4);
      day5Text = (TextView) itemView.findViewById(R.id.tv__day_5);
      day6Text = (TextView) itemView.findViewById(R.id.tv__day_6);
      day7Text = (TextView) itemView.findViewById(R.id.tv__day_7);
      // FIXME @yunik localize texts
      day1Text.setText("Su");
      day2Text.setText("Mo");
      day3Text.setText("Tu");
      day4Text.setText("We");
      day5Text.setText("Th");
      day6Text.setText("Fr");
      day7Text.setText("Sa");
    }

    private void checkWithoutCallback(CompoundButton button, boolean check)
    {
      button.setOnCheckedChangeListener(null);
      button.setChecked(check);
      button.setOnCheckedChangeListener(this);
    }
  }

  private class AddTimetableViewHolder extends BaseTimetableViewHolder
  {
    private TextView add;

    public AddTimetableViewHolder(View itemView)
    {
      super(itemView);
      add = (TextView) itemView.findViewById(R.id.btn__add_time);
      add.setOnClickListener(new View.OnClickListener()
      {
        @Override
        public void onClick(View v)
        {
          addTimetable();
        }
      });
    }

    @Override
    void onBind()
    {
      // TODO @yunik add text with complement days
      add.setEnabled(mComplementItem != null && mComplementItem.mWeekdays.length != 0);
    }
  }
}
