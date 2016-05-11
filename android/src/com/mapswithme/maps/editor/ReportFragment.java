package com.mapswithme.maps.editor;

import android.os.Bundle;
import android.support.annotation.IntRange;
import android.support.annotation.Nullable;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;

import com.mapswithme.maps.R;
import com.mapswithme.maps.base.BaseMwmToolbarFragment;
import com.mapswithme.util.UiUtils;

public class ReportFragment extends BaseMwmToolbarFragment implements View.OnClickListener
{
  private View mSimpleProblems;
  private View mAdvancedProblem;
  private View mSave;
  private EditText mProblemInput;

  private boolean mAdvancedMode;
  @IntRange(from = 0, to = 3)
  private int mSelectedProblem;

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState)
  {
    return inflater.inflate(R.layout.fragment_report, container, false);
  }

  @Override
  public void onViewCreated(View view, @Nullable Bundle savedInstanceState)
  {
    super.onViewCreated(view, savedInstanceState);
    mToolbarController.setTitle(R.string.editor_report_problem_title);

    mSave = mToolbarController.findViewById(R.id.save);
    mSave.setOnClickListener(this);
    mSimpleProblems = view.findViewById(R.id.ll__problems);
    mSimpleProblems.findViewById(R.id.problem_not_exist).setOnClickListener(this);
//    mSimpleProblems.findViewById(R.id.problem_closed_repair).setOnClickListener(this);
//    mSimpleProblems.findViewById(R.id.problem_duplicated_place).setOnClickListener(this);
    mSimpleProblems.findViewById(R.id.problem_other).setOnClickListener(this);
    mAdvancedProblem = view.findViewById(R.id.ll__other_problem);
    mProblemInput = (EditText) mAdvancedProblem.findViewById(R.id.input);
    refreshProblems();
  }

  private void refreshProblems()
  {
    UiUtils.showIf(mAdvancedMode, mAdvancedProblem, mSave);
    UiUtils.showIf(!mAdvancedMode, mSimpleProblems);
  }

  private void send(String text)
  {
    Editor.nativeCreateNote(text);
    mToolbarController.onUpClick();
  }

  private void sendNotExist()
  {
    Editor.nativePlaceDoesNotExist("");
    mToolbarController.onUpClick();
  }

  @Override
  public void onClick(View v)
  {
    switch (v.getId())
    {
    case R.id.problem_not_exist:
//    case R.id.problem_closed_repair:
//    case R.id.problem_duplicated_place:
      sendNotExist();
      break;

    case R.id.problem_other:
      mAdvancedMode = true;
      refreshProblems();
      break;

    case R.id.save:
      String text = mProblemInput.getText().toString().trim();
      if (TextUtils.isEmpty(text))
        return;

      send(text);
      break;
    }
  }
}
