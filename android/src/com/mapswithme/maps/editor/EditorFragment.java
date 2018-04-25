package com.mapswithme.maps.editor;

import android.content.DialogInterface;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.support.annotation.CallSuper;
import android.support.annotation.DrawableRes;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.StringRes;
import android.support.design.widget.TextInputLayout;
import android.support.v7.app.AlertDialog;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.SwitchCompat;
import android.text.InputType;
import android.text.TextUtils;
import android.util.SparseArray;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.TextView;

import com.mapswithme.maps.Framework;
import com.mapswithme.maps.R;
import com.mapswithme.maps.base.BaseMwmFragment;
import com.mapswithme.maps.bookmarks.data.Metadata.MetadataType;
import com.mapswithme.maps.dialog.EditTextDialogFragment;
import com.mapswithme.maps.editor.data.LocalizedName;
import com.mapswithme.maps.editor.data.LocalizedStreet;
import com.mapswithme.maps.editor.data.TimeFormatUtils;
import com.mapswithme.maps.editor.data.Timetable;
import com.mapswithme.util.Constants;
import com.mapswithme.util.Graphics;
import com.mapswithme.util.InputUtils;
import com.mapswithme.util.StringUtils;
import com.mapswithme.util.UiUtils;
import org.solovyev.android.views.llm.LinearLayoutManager;

public class EditorFragment extends BaseMwmFragment implements View.OnClickListener,
                                                               EditTextDialogFragment.EditTextDialogInterface
{
  final static String LAST_INDEX_OF_NAMES_ARRAY = "LastIndexOfNamesArray";

  private TextView mCategory;
  private View mCardName;
  private View mCardAddress;
  private View mCardMetadata;

  private RecyclerView mNamesView;

  private final RecyclerView.AdapterDataObserver mNamesObserver = new RecyclerView.AdapterDataObserver()
  {
    @Override
    public void onChanged()
    {
      refreshNamesCaption();
    }

    @Override
    public void onItemRangeChanged(int positionStart, int itemCount)
    {
      refreshNamesCaption();
    }

    @Override
    public void onItemRangeInserted(int positionStart, int itemCount)
    {
      refreshNamesCaption();
    }

    @Override
    public void onItemRangeRemoved(int positionStart, int itemCount)
    {
      refreshNamesCaption();
    }

    @Override
    public void onItemRangeMoved(int fromPosition, int toPosition, int itemCount)
    {
      refreshNamesCaption();
    }
  };

  private MultilanguageAdapter mNamesAdapter;
  private TextView mNamesCaption;
  private TextView mAddLanguage;
  private TextView mMoreLanguages;

  private TextView mStreet;
  private EditText mHouseNumber;
  private EditText mZipcode;
  private View mBlockLevels;
  private EditText mBuildingLevels;
  private EditText mPhone;
  private EditText mWebsite;
  private EditText mEmail;
  private TextView mCuisine;
  private EditText mOperator;
  private SwitchCompat mWifi;

  private TextInputLayout mInputHouseNumber;
  private TextInputLayout mInputBuildingLevels;
  private TextInputLayout mInputZipcode;
  private TextInputLayout mInputPhone;
  private TextInputLayout mInputWebsite;
  private TextInputLayout mInputEmail;

  private View mEmptyOpeningHours;
  private TextView mOpeningHours;
  private View mEditOpeningHours;
  private EditText mDescription;
  private final SparseArray<View> mMetaBlocks = new SparseArray<>(7);
  private TextView mReset;

  private EditorHostFragment mParent;

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState)
  {
    return inflater.inflate(R.layout.fragment_editor, container, false);
  }

  @CallSuper
  @Override
  public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState)
  {
    mParent = (EditorHostFragment) getParentFragment();

    initViews(view);

    mCategory.setText(Editor.nativeGetCategory());
    final LocalizedStreet street = Editor.nativeGetStreet();
    mStreet.setText(street.defaultName);

    mHouseNumber.setText(Editor.nativeGetHouseNumber());
    mHouseNumber.addTextChangedListener(new StringUtils.SimpleTextWatcher()
    {
      @Override
      public void onTextChanged(CharSequence s, int start, int before, int count)
      {
        UiUtils.setInputError(mInputHouseNumber, Editor.nativeIsHouseValid(s.toString()) ? 0 : R.string.error_enter_correct_house_number);
      }
    });

    mZipcode.setText(Editor.nativeGetZipCode());
    mZipcode.addTextChangedListener(new StringUtils.SimpleTextWatcher()
    {
      @Override
      public void onTextChanged(CharSequence s, int start, int before, int count)
      {
        UiUtils.setInputError(mInputZipcode, Editor.nativeIsZipcodeValid(s.toString()) ? 0 : R.string.error_enter_correct_zip_code);
      }
    });

    mBuildingLevels.setText(Editor.nativeGetBuildingLevels());
    mBuildingLevels.addTextChangedListener(new StringUtils.SimpleTextWatcher()
    {
      @Override
      public void onTextChanged(CharSequence s, int start, int before, int count)
      {
        UiUtils.setInputError(mInputBuildingLevels, Editor.nativeIsLevelValid(s.toString()) ? 0 : R.string.error_enter_correct_storey_number);
      }
    });

    mPhone.setText(Editor.nativeGetPhone());
    mPhone.addTextChangedListener(new StringUtils.SimpleTextWatcher()
    {
      @Override
      public void onTextChanged(CharSequence s, int start, int before, int count)
      {
        UiUtils.setInputError(mInputPhone, Editor.nativeIsPhoneValid(s.toString()) ? 0 : R.string.error_enter_correct_phone);
      }
    });

    mWebsite.setText(Editor.nativeGetWebsite());
    mWebsite.addTextChangedListener(new StringUtils.SimpleTextWatcher()
    {
      @Override
      public void onTextChanged(CharSequence s, int start, int before, int count)
      {
        UiUtils.setInputError(mInputWebsite, Editor.nativeIsWebsiteValid(s.toString()) ? 0 : R.string.error_enter_correct_web);
      }
    });

    mEmail.setText(Editor.nativeGetEmail());
    mEmail.addTextChangedListener(new StringUtils.SimpleTextWatcher()
    {
      @Override
      public void onTextChanged(CharSequence s, int start, int before, int count)
      {
        UiUtils.setInputError(mInputEmail, Editor.nativeIsEmailValid(s.toString()) ? 0 : R.string.error_enter_correct_email);
      }
    });

    mCuisine.setText(Editor.nativeGetFormattedCuisine());
    mOperator.setText(Editor.nativeGetOperator());
    mWifi.setChecked(Editor.nativeHasWifi());
    refreshOpeningTime();
    refreshEditableFields();
    refreshResetButton();
  }

  @Override
  public void onSaveInstanceState(Bundle outState)
  {
    super.onSaveInstanceState(outState);
    setEdits();
  }

  boolean setEdits()
  {
    if (!validateFields())
      return false;

    Editor.nativeSetHouseNumber(mHouseNumber.getText().toString());
    Editor.nativeSetZipCode(mZipcode.getText().toString());
    Editor.nativeSetBuildingLevels(mBuildingLevels.getText().toString());
    Editor.nativeSetPhone(mPhone.getText().toString());
    Editor.nativeSetWebsite(mWebsite.getText().toString());
    Editor.nativeSetEmail(mEmail.getText().toString());
    Editor.nativeSetHasWifi(mWifi.isChecked());
    Editor.nativeSetOperator(mOperator.getText().toString());
    Editor.nativeSetNames(mParent.getNamesAsArray());

    return true;
  }

  @NonNull
  protected String getDescription()
  {
    return mDescription.getText().toString().trim();
  }

  private boolean validateFields()
  {
    if (Editor.nativeIsAddressEditable())
    {
      if (!Editor.nativeIsHouseValid(mHouseNumber.getText().toString()))
      {
        mHouseNumber.requestFocus();
        InputUtils.showKeyboard(mHouseNumber);
        return false;
      }

      if (!Editor.nativeIsLevelValid(mBuildingLevels.getText().toString()))
      {
        mBuildingLevels.requestFocus();
        InputUtils.showKeyboard(mBuildingLevels);
        return false;
      }
    }

    if (!Editor.nativeIsZipcodeValid(mZipcode.getText().toString()))
    {
      mZipcode.requestFocus();
      InputUtils.showKeyboard(mZipcode);
      return false;
    }

    if (!Editor.nativeIsPhoneValid(mPhone.getText().toString()))
    {
      mPhone.requestFocus();
      InputUtils.showKeyboard(mPhone);
      return false;
    }

    if (!Editor.nativeIsWebsiteValid(mWebsite.getText().toString()))
    {
      mWebsite.requestFocus();
      InputUtils.showKeyboard(mWebsite);
      return false;
    }

    if (!Editor.nativeIsEmailValid(mEmail.getText().toString()))
    {
      mEmail.requestFocus();
      InputUtils.showKeyboard(mEmail);
      return false;
    }

    return validateNames();
  }

  private boolean validateNames()
  {
    for (int pos = 0; pos < mNamesAdapter.getItemCount(); pos++)
    {
      LocalizedName localizedName = mNamesAdapter.getNameAtPos(pos);
      if (Editor.nativeIsNameValid(localizedName.name))
        continue;

      View nameView = mNamesView.getChildAt(pos);
      nameView.requestFocus();

      InputUtils.showKeyboard(nameView);

      return false;
    }

    return true;
  }

  private void refreshEditableFields()
  {
    UiUtils.showIf(Editor.nativeIsNameEditable(), mCardName);
    UiUtils.showIf(Editor.nativeIsAddressEditable(), mCardAddress);
    UiUtils.showIf(Editor.nativeIsBuilding() && !Editor.nativeIsPointType(), mBlockLevels);

    final int[] editableMeta = Editor.nativeGetEditableFields();
    if (editableMeta.length == 0)
    {
      UiUtils.hide(mCardMetadata);
      return;
    }

    for (int i = 0; i < mMetaBlocks.size(); i++)
      UiUtils.hide(mMetaBlocks.valueAt(i));

    boolean anyEditableMeta = false;
    for (int type : editableMeta)
    {
      final View metaBlock = mMetaBlocks.get(type);
      if (metaBlock == null)
        continue;

      anyEditableMeta = true;
      UiUtils.show(metaBlock);
    }
    UiUtils.showIf(anyEditableMeta, mCardMetadata);
  }

  private void refreshOpeningTime()
  {
    final Timetable[] timetables = OpeningHours.nativeTimetablesFromString(Editor.nativeGetOpeningHours());
    if (timetables == null)
    {
      UiUtils.show(mEmptyOpeningHours);
      UiUtils.hide(mOpeningHours, mEditOpeningHours);
    }
    else
    {
      UiUtils.hide(mEmptyOpeningHours);
      UiUtils.setTextAndShow(mOpeningHours, TimeFormatUtils.formatTimetables(timetables));
      UiUtils.show(mEditOpeningHours);
    }
  }

  private void initNamesView(final View view)
  {
    mNamesCaption = (TextView) view.findViewById(R.id.show_additional_names);
    mNamesCaption.setOnClickListener(this);

    mAddLanguage = (TextView) view.findViewById(R.id.add_langs);
    mAddLanguage.setOnClickListener(this);

    mMoreLanguages = (TextView) view.findViewById(R.id.more_names);
    mMoreLanguages.setOnClickListener(this);

    mNamesView = (RecyclerView) view.findViewById(R.id.recycler);
    mNamesView.setNestedScrollingEnabled(false);
    mNamesView.setLayoutManager(new LinearLayoutManager(getActivity()));
    mNamesAdapter = new MultilanguageAdapter(mParent);
    mNamesView.setAdapter(mNamesAdapter);
    mNamesAdapter.registerAdapterDataObserver(mNamesObserver);

    final Bundle args = getArguments();
    if (args == null || !args.containsKey(LAST_INDEX_OF_NAMES_ARRAY))
    {
      showAdditionalNames(false);
      return;
    }
    showAdditionalNames(true);
    UiUtils.waitLayout(mNamesView, new ViewTreeObserver.OnGlobalLayoutListener()
    {
      @Override
      public void onGlobalLayout()
      {
        LinearLayoutManager lm = (LinearLayoutManager) mNamesView.getLayoutManager();
        int position = args.getInt(LAST_INDEX_OF_NAMES_ARRAY);

        View nameItem = lm.findViewByPosition(position);

        int cvNameTop = mCardName.getTop();
        int nameItemTop = nameItem.getTop();

        view.scrollTo(0, cvNameTop + nameItemTop);

        // TODO(mgsergio): Uncomment if focus and keyboard are required.
        // TODO(mgsergio): Keyboard doesn't want to hide. Only pressing back button works.
        // View nameItemInput = nameItem.findViewById(R.id.input);
        // nameItemInput.requestFocus();
        // InputUtils.showKeyboard(nameItemInput);
      }
    });
  }

  private void initViews(View view)
  {
    final View categoryBlock = view.findViewById(R.id.category);
    categoryBlock.setOnClickListener(this);
    // TODO show icon and fill it when core will implement that
    UiUtils.hide(categoryBlock.findViewById(R.id.icon));
    mCategory = (TextView) categoryBlock.findViewById(R.id.name);
    mCardName = view.findViewById(R.id.cv__name);
    mCardAddress = view.findViewById(R.id.cv__address);
    mCardMetadata = view.findViewById(R.id.cv__metadata);
    initNamesView(view);

    // Address
    view.findViewById(R.id.block_street).setOnClickListener(this);
    mStreet = (TextView) view.findViewById(R.id.street);
    View blockHouseNumber = view.findViewById(R.id.block_building);
    mHouseNumber = findInputAndInitBlock(blockHouseNumber, 0, R.string.house_number);
    mInputHouseNumber = (TextInputLayout) blockHouseNumber.findViewById(R.id.custom_input);
    View blockZipcode = view.findViewById(R.id.block_zipcode);
    mZipcode = findInputAndInitBlock(blockZipcode, 0, R.string.editor_zip_code);
    mInputZipcode = (TextInputLayout) blockZipcode.findViewById(R.id.custom_input);

    // Details
    mBlockLevels = view.findViewById(R.id.block_levels);
    mBuildingLevels = findInputAndInitBlock(mBlockLevels, 0, getString(R.string.editor_storey_number, 25));
    mBuildingLevels.setInputType(InputType.TYPE_CLASS_NUMBER);
    mInputBuildingLevels = (TextInputLayout) mBlockLevels.findViewById(R.id.custom_input);
    View blockPhone = view.findViewById(R.id.block_phone);
    mPhone = findInputAndInitBlock(blockPhone, R.drawable.ic_phone, R.string.phone);
    mPhone.setInputType(InputType.TYPE_CLASS_PHONE);
    mInputPhone = (TextInputLayout) blockPhone.findViewById(R.id.custom_input);
    View blockWeb = view.findViewById(R.id.block_website);
    mWebsite = findInputAndInitBlock(blockWeb, R.drawable.ic_website, R.string.website);
    mWebsite.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
    mInputWebsite = (TextInputLayout) blockWeb.findViewById(R.id.custom_input);
    View blockEmail = view.findViewById(R.id.block_email);
    mEmail = findInputAndInitBlock(blockEmail, R.drawable.ic_email, R.string.email);
    mEmail.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS);
    mInputEmail = (TextInputLayout) blockEmail.findViewById(R.id.custom_input);
    View blockCuisine = view.findViewById(R.id.block_cuisine);
    blockCuisine.setOnClickListener(this);
    mCuisine = (TextView) view.findViewById(R.id.cuisine);
    View blockOperator = view.findViewById(R.id.block_operator);
    mOperator = findInputAndInitBlock(blockOperator, R.drawable.ic_operator, R.string.editor_operator);
    View blockWifi = view.findViewById(R.id.block_wifi);
    mWifi = (SwitchCompat) view.findViewById(R.id.sw__wifi);
    blockWifi.setOnClickListener(this);
    View blockOpeningHours = view.findViewById(R.id.block_opening_hours);
    mEditOpeningHours = blockOpeningHours.findViewById(R.id.edit_opening_hours);
    mEditOpeningHours.setOnClickListener(this);
    mEmptyOpeningHours = blockOpeningHours.findViewById(R.id.empty_opening_hours);
    mEmptyOpeningHours.setOnClickListener(this);
    mOpeningHours = (TextView) blockOpeningHours.findViewById(R.id.opening_hours);
    mOpeningHours.setOnClickListener(this);
    final View cardMore = view.findViewById(R.id.cv__more);
    mDescription = findInput(cardMore);
    cardMore.findViewById(R.id.about_osm).setOnClickListener(this);
    mReset = (TextView) view.findViewById(R.id.reset);
    mReset.setOnClickListener(this);

    mMetaBlocks.append(MetadataType.FMD_OPEN_HOURS.toInt(), blockOpeningHours);
    mMetaBlocks.append(MetadataType.FMD_PHONE_NUMBER.toInt(), blockPhone);
    mMetaBlocks.append(MetadataType.FMD_WEBSITE.toInt(), blockWeb);
    mMetaBlocks.append(MetadataType.FMD_EMAIL.toInt(), blockEmail);
    mMetaBlocks.append(MetadataType.FMD_CUISINE.toInt(), blockCuisine);
    mMetaBlocks.append(MetadataType.FMD_OPERATOR.toInt(), blockOperator);
    mMetaBlocks.append(MetadataType.FMD_INTERNET.toInt(), blockWifi);
  }

  private static EditText findInput(View blockWithInput)
  {
    return (EditText) blockWithInput.findViewById(R.id.input);
  }

  private EditText findInputAndInitBlock(View blockWithInput, @DrawableRes int icon, @StringRes int hint)
  {
    return findInputAndInitBlock(blockWithInput, icon, getString(hint));
  }

  private static EditText findInputAndInitBlock(View blockWithInput, @DrawableRes int icon, String hint)
  {
    ((ImageView) blockWithInput.findViewById(R.id.icon)).setImageResource(icon);
    final TextInputLayout input = (TextInputLayout) blockWithInput.findViewById(R.id.custom_input);
    input.setHint(hint);
    return (EditText) input.findViewById(R.id.input);
  }

  @Override
  public void onClick(View v)
  {
    switch (v.getId())
    {
    case R.id.edit_opening_hours:
    case R.id.empty_opening_hours:
    case R.id.opening_hours:
      mParent.editTimetable();
      break;
    case R.id.block_wifi:
      mWifi.toggle();
      break;
    case R.id.block_street:
      mParent.editStreet();
      break;
    case R.id.block_cuisine:
      mParent.editCuisine();
      break;
    case R.id.category:
      mParent.editCategory();
      break;
    case R.id.more_names:
    case R.id.show_additional_names:
      if (mNamesAdapter.areAdditionalLanguagesShown() && !validateNames())
        break;
      showAdditionalNames(!mNamesAdapter.areAdditionalLanguagesShown());
      break;
    case R.id.add_langs:
      mParent.addLanguage();
      break;
    case R.id.about_osm:
      startActivity(new Intent((Intent.ACTION_VIEW), Uri.parse(Constants.Url.OSM_ABOUT)));
      break;
    case R.id.reset:
      reset();
      break;
    }
  }

  private void showAdditionalNames(boolean show)
  {
    mNamesAdapter.showAdditionalLanguages(show);

    refreshNamesCaption();
  }

  private void refreshNamesCaption()
  {
    if (mNamesAdapter.getNamesCount() <= mNamesAdapter.getMandatoryNamesCount())
      setNamesArrow(0 /* arrowResourceId */);  // bind arrow with empty resource (do not draw arrow)
    else if (mNamesAdapter.areAdditionalLanguagesShown())
      setNamesArrow(R.drawable.ic_expand_less);
    else
      setNamesArrow(R.drawable.ic_expand_more);

    boolean showAddLanguage = mNamesAdapter.getNamesCount() <= mNamesAdapter.getMandatoryNamesCount() ||
      mNamesAdapter.areAdditionalLanguagesShown();

    UiUtils.showIf(showAddLanguage, mAddLanguage);
    UiUtils.showIf(!showAddLanguage, mMoreLanguages);
  }

  // Bind arrow in the top right corner of names caption with needed resource.
  private void setNamesArrow(@DrawableRes int arrowResourceId)
  {
    if (arrowResourceId == 0)
    {
      mNamesCaption.setCompoundDrawablesWithIntrinsicBounds(null, null, null, null);
      return;
    }

    mNamesCaption.setCompoundDrawablesWithIntrinsicBounds(
      null,
      null,
      Graphics.tint(getActivity(), arrowResourceId, R.attr.iconTint),
      null);
  }

  private void refreshResetButton()
  {
    if (mParent.addingNewObject())
    {
      UiUtils.hide(mReset);
      return;
    }

    if (Editor.nativeIsMapObjectUploaded())
    {
      mReset.setText(R.string.editor_place_doesnt_exist);
      return;
    }

    switch (Editor.nativeGetMapObjectStatus())
    {
    case Editor.CREATED:
      mReset.setText(R.string.editor_remove_place_button);
      break;
    case Editor.MODIFIED:
      mReset.setText(R.string.editor_reset_edits_button);
      break;
    case Editor.UNTOUCHED:
      mReset.setText(R.string.editor_place_doesnt_exist);
      break;
    case Editor.DELETED:
      throw new IllegalStateException("Can't delete already deleted feature.");
    case Editor.OBSOLETE:
      throw new IllegalStateException("Obsolete objects cannot be reverted.");
    }
  }

  private void reset()
  {
    if (Editor.nativeIsMapObjectUploaded())
    {
      placeDoesntExist();
      return;
    }

    switch (Editor.nativeGetMapObjectStatus())
    {
    case Editor.CREATED:
      rollback(Editor.CREATED);
      break;
    case Editor.MODIFIED:
      rollback(Editor.MODIFIED);
      break;
    case Editor.UNTOUCHED:
      placeDoesntExist();
      break;
    case Editor.DELETED:
      throw new IllegalStateException("Can't delete already deleted feature.");
    case Editor.OBSOLETE:
      throw new IllegalStateException("Obsolete objects cannot be reverted.");
    }
  }

  private void rollback(@Editor.FeatureStatus int status)
  {
    int title;
    int message;
    if (status == Editor.CREATED)
    {
      title = R.string.editor_remove_place_button;
      message = R.string.editor_remove_place_message;
    }
    else
    {
      title = R.string.editor_reset_edits_button;
      message = R.string.editor_reset_edits_message;
    }

    new AlertDialog.Builder(getActivity()).setTitle(message)
                                          .setPositiveButton(getString(title).toUpperCase(), new DialogInterface.OnClickListener()
                                          {
                                            @Override
                                            public void onClick(DialogInterface dialog, int which)
                                            {
                                              Editor.nativeRollbackMapObject();
                                              Framework.nativePokeSearchInViewport();
                                              mParent.onBackPressed();
                                            }
                                          })
                                          .setNegativeButton(getString(R.string.cancel).toUpperCase(), null)
                                          .show();
  }

  private void placeDoesntExist()
  {
    EditTextDialogFragment.show(getString(R.string.editor_place_doesnt_exist), "", getString(R.string.editor_comment_hint),
                                getString(R.string.editor_report_problem_send_button), getString(R.string.cancel), this);
  }

  @NonNull
  @Override
  public EditTextDialogFragment.OnTextSaveListener getSaveTextListener()
  {
    return text -> {
      Editor.nativePlaceDoesNotExist(text);
      mParent.onBackPressed();
    };
  }

  @NonNull
  @Override
  public EditTextDialogFragment.Validator getValidator()
  {
    return (activity, text) -> !TextUtils.isEmpty(text);
  }
}
