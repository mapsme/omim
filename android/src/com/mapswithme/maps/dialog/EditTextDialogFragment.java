package com.mapswithme.maps.dialog;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.Dialog;
import android.content.DialogInterface;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.StringRes;
import android.support.design.widget.TextInputLayout;
import android.support.v4.app.Fragment;
import android.support.v7.app.AlertDialog;
import android.text.TextUtils;
import android.view.View;
import android.widget.EditText;
import android.widget.TextView;

import com.mapswithme.maps.R;
import com.mapswithme.maps.base.BaseMwmDialogFragment;
import com.mapswithme.util.InputUtils;
import com.mapswithme.util.UiUtils;

public class EditTextDialogFragment extends BaseMwmDialogFragment
{
  public static final String EXTRA_TITLE = "Title";
  public static final String EXTRA_MESSAGE = "Message";
  public static final String EXTRA_INITIAL = "InitialText";
  public static final String EXTRA_POSITIVE_BUTTON = "PositiveText";
  public static final String EXTRA_NEGATIVE_BUTTON = "NegativeText";
  public static final String EXTRA_HINT = "Hint";

  private String mTitle;
  private String mMessage;
  private String mInitialText;
  private String mHint;
  private EditText mEtInput;

  public interface OnTextSaveListener
  {
    void onSaveText(String text);
  }

  @NonNull
  @Override
  public Dialog onCreateDialog(Bundle savedInstanceState)
  {
    final Bundle args = getArguments();
    String positiveButtonText = getString(R.string.ok);
    String negativeButtonText = getString(R.string.cancel);
    if (args != null)
    {
      mTitle = args.getString(EXTRA_TITLE);
      mMessage = args.getString(EXTRA_MESSAGE);
      mInitialText = args.getString(EXTRA_INITIAL);
      mHint = args.getString(EXTRA_HINT);

      positiveButtonText = args.getString(EXTRA_POSITIVE_BUTTON);
      negativeButtonText = args.getString(EXTRA_NEGATIVE_BUTTON);
    }

    return new AlertDialog.Builder(getActivity())
               .setView(buildView())
               .setNegativeButton(negativeButtonText, null)
               .setPositiveButton(positiveButtonText, new DialogInterface.OnClickListener()
               {
                 @Override
                 public void onClick(DialogInterface dialog, int which)
                 {
                   final Fragment parentFragment = getParentFragment();
                   final String result = mEtInput.getText().toString();
                   if (parentFragment instanceof OnTextSaveListener)
                   {
                     dismiss();
                     ((OnTextSaveListener) parentFragment).onSaveText(result);
                     return;
                   }

                   final Activity activity = getActivity();
                   if (activity instanceof OnTextSaveListener)
                     ((OnTextSaveListener) activity).onSaveText(result);
                 }
               }).create();
  }

  private View buildView()
  {
    @SuppressLint("InflateParams")
    final View root = getActivity().getLayoutInflater().inflate(R.layout.dialog_edit_text, null);
    TextInputLayout inputLayout = (TextInputLayout) root.findViewById(R.id.input);
    inputLayout.setHint(mHint);
    mEtInput = (EditText) inputLayout.findViewById(R.id.et__input);
    if (!TextUtils.isEmpty(mInitialText))
    {
      mEtInput.setText(mInitialText);
      mEtInput.selectAll();
    }
    InputUtils.showKeyboard(mEtInput);

    ((TextView) root.findViewById(R.id.tv__title)).setText(mTitle);
    UiUtils.setTextAndHideIfEmpty((TextView) root.findViewById(R.id.tv__message), mMessage);
    return root;
  }

  public static class Builder
  {
    private final Fragment mParent;
    private Bundle mArgs;

    public Builder(Fragment parent)
    {
      mParent = parent;
      mArgs = new Bundle();
    }

    public Builder title(@StringRes int title)
    {
      return title(mParent.getString(title));
    }

    public Builder title(String title)
    {
      mArgs.putString(EXTRA_TITLE, title);
      return this;
    }

    public Builder message(@StringRes int message)
    {
      return message(mParent.getString(message));
    }

    public Builder message(String message)
    {
      mArgs.putString(EXTRA_MESSAGE, message);
      return this;
    }

    public Builder hint(@StringRes int hint)
    {
      return hint(mParent.getString(hint));
    }

    public Builder hint(String hint)
    {
      mArgs.putString(EXTRA_HINT, hint);
      return this;
    }

    public Builder initialText(@StringRes int initialText)
    {
      return initialText(mParent.getString(initialText));
    }

    public Builder initialText(String initialText)
    {
      mArgs.putString(EXTRA_INITIAL, initialText);
      return this;
    }

    public Builder positiveButton(@StringRes int positiveButton)
    {
      return positiveButton(mParent.getString(positiveButton));
    }

    public Builder positiveButton(String positiveButton)
    {
      mArgs.putString(EXTRA_POSITIVE_BUTTON, positiveButton.toUpperCase());
      return this;
    }

    public Builder negativeButton(@StringRes int negativeButton)
    {
      return negativeButton(mParent.getString(negativeButton));
    }

    public Builder negativeButton(String negativeButton)
    {
      mArgs.putString(EXTRA_NEGATIVE_BUTTON, negativeButton.toUpperCase());
      return this;
    }

    public void show()
    {
      final EditTextDialogFragment fragment = (EditTextDialogFragment) Fragment.instantiate(
          mParent.getActivity(), EditTextDialogFragment.class.getName());
      fragment.setArguments(mArgs);
      fragment.show(mParent.getChildFragmentManager(), EditTextDialogFragment.class.getName());
    }
  }
}
