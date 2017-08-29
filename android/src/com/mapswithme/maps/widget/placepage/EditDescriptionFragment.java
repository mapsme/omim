package com.mapswithme.maps.widget.placepage;

import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v7.widget.Toolbar;
import android.text.Html;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.EditText;
import android.widget.TextView;

import java.lang.ref.WeakReference;

import com.mapswithme.maps.R;
import com.mapswithme.maps.base.BaseMwmDialogFragment;
import com.mapswithme.maps.bookmarks.data.Bookmark;
import com.mapswithme.util.StringUtils;
import com.mapswithme.util.UiUtils;

public class EditDescriptionFragment extends BaseMwmDialogFragment
{
  public static final String EXTRA_BOOKMARK = "bookmark";

  private EditText mEtDescription;
  private Bookmark mBookmark;

  public interface OnDescriptionSavedListener
  {
    void onSaved(Bookmark bookmark);
  }

  private WeakReference<OnDescriptionSavedListener> mListener;

  public EditDescriptionFragment() {}

  @Override
  protected int getCustomTheme()
  {
    return getFullscreenTheme();
  }

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
  {
    return inflater.inflate(R.layout.fragment_edit_description, container, false);
  }

  @Override
  public void onViewCreated(@NonNull View view, Bundle savedInstanceState)
  {
    mBookmark = getArguments().getParcelable(EXTRA_BOOKMARK);
    String description = null;
    if (mBookmark != null)
      description = mBookmark.getBookmarkDescription();

    if (description != null && StringUtils.nativeIsHtml(description))
    {
      final String descriptionNoSimpleTags = StringUtils.removeEditTextHtmlTags(description);
      if (!StringUtils.nativeIsHtml(descriptionNoSimpleTags))
        description = Html.fromHtml(description).toString();
    }

    mEtDescription = (EditText) view.findViewById(R.id.et__description);
    mEtDescription.setText(description);
    initToolbar(view);

    mEtDescription.requestFocus();
    getDialog().getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_VISIBLE);
  }

  public void setSaveDescriptionListener(OnDescriptionSavedListener listener)
  {
    mListener = new WeakReference<>(listener);
  }

  private void initToolbar(View view)
  {
    Toolbar toolbar = (Toolbar) view.findViewById(R.id.toolbar);
    UiUtils.extendViewWithStatusBar(toolbar);
    final TextView textView = (TextView) toolbar.findViewById(R.id.tv__save);
    textView.setOnClickListener(new View.OnClickListener()
    {
      @Override
      public void onClick(View v)
      {
        saveDescription();
      }
    });
    UiUtils.showHomeUpButton(toolbar);
    toolbar.setTitle(R.string.description);
    toolbar.setNavigationOnClickListener(new View.OnClickListener()
    {
      @Override
      public void onClick(View v)
      {
        dismiss();
      }
    });
  }

  private void saveDescription()
  {
    mBookmark.setParams(mBookmark.getTitle(), null, mEtDescription.getText().toString());

    if (mListener != null)
    {
      OnDescriptionSavedListener listener = mListener.get();
      if (listener != null)
        listener.onSaved(mBookmark);
    }
    dismiss();
  }

  @Override
  public void onDetach()
  {
    super.onDetach();
    mListener = null;
  }
}
