package com.mapswithme.maps.news;

import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.os.Bundle;
import android.support.annotation.IdRes;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.StringRes;
import android.support.annotation.StyleRes;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.FragmentManager;
import android.text.Html;
import android.text.Spanned;
import android.text.method.LinkMovementMethod;
import android.view.View;
import android.view.Window;
import android.widget.CheckBox;
import android.widget.ImageView;
import android.widget.TextView;

import com.mapswithme.maps.BuildConfig;
import com.mapswithme.maps.Framework;
import com.mapswithme.maps.MwmApplication;
import com.mapswithme.maps.R;
import com.mapswithme.maps.base.BaseMwmDialogFragment;
import com.mapswithme.maps.location.LocationHelper;
import com.mapswithme.util.Constants;
import com.mapswithme.util.Counters;
import com.mapswithme.util.UiUtils;

public class WelcomeDialogFragment extends BaseMwmDialogFragment implements View.OnClickListener
{
  @Nullable
  private BaseNewsFragment.NewsDialogListener mListener;
  @SuppressWarnings("all")
  @NonNull
  private View mRootView;
  @SuppressWarnings("all")
  @NonNull
  private CheckBox mTermsCheckbox;
  @SuppressWarnings("all")
  @NonNull
  private CheckBox mPrivacyPolicyCheckbox;

  public static boolean showOn(@NonNull FragmentActivity activity)
  {
    if (Counters.getFirstInstallVersion() < BuildConfig.VERSION_CODE)
      return false;

    FragmentManager fm = activity.getSupportFragmentManager();
    if (fm.isDestroyed())
      return false;

    if (Counters.isFirstStartDialogSeen()
        && !recreate(activity)
        && isPrivacyPolicyConfirm(activity)
        && isTermOfUseConfirm(activity))
      return false;

    create(activity);

    Counters.setFirstStartDialogSeen();

    return true;
  }

  private static void create(@NonNull FragmentActivity activity)
  {
    final WelcomeDialogFragment fragment = new WelcomeDialogFragment();
    activity.getSupportFragmentManager()
            .beginTransaction()
            .add(fragment, WelcomeDialogFragment.class.getName())
            .commitAllowingStateLoss();
  }

  private static boolean recreate(@NonNull FragmentActivity activity)
  {
    FragmentManager fm = activity.getSupportFragmentManager();
    Fragment f = fm.findFragmentByTag(WelcomeDialogFragment.class.getName());
    if (f == null)
      return false;

    // If we're here, it means that the user has rotated the screen.
    // We use different dialog themes for landscape and portrait modes on tablets,
    // so the fragment should be recreated to be displayed correctly.
    fm.beginTransaction()
      .remove(f)
      .commitAllowingStateLoss();
    fm.executePendingTransactions();
    return true;
  }

  @Override
  public void onAttach(Activity activity)
  {
    super.onAttach(activity);
    if (activity instanceof BaseNewsFragment.NewsDialogListener)
      mListener = (BaseNewsFragment.NewsDialogListener) activity;
  }

  @Override
  public void onDetach()
  {
    mListener = null;
    super.onDetach();
  }

  @Override
  protected int getCustomTheme()
  {
    return getFullscreenTheme();
  }

  @NonNull
  @Override
  public Dialog onCreateDialog(Bundle savedInstanceState)
  {
    LocationHelper.INSTANCE.onEnteredIntoFirstRun();
    if (!LocationHelper.INSTANCE.isActive())
      LocationHelper.INSTANCE.start();

    Dialog dialog = super.onCreateDialog(savedInstanceState);
    dialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
    dialog.setCancelable(false);

    mRootView = View.inflate(getActivity(), R.layout.fragment_welcome, null);
    dialog.setContentView(mRootView);


    ImageView image = mRootView.findViewById(R.id.iv__image);
    image.setImageResource(R.drawable.img_welcome);
    TextView title = mRootView.findViewById(R.id.tv__title);
    title.setText(R.string.onboarding_welcome_title);
    TextView subtitle = mRootView.findViewById(R.id.tv__subtitle1);
    subtitle.setText(R.string.onboarding_welcome_first_subtitle);

    UiUtils.show(mRootView.findViewById(R.id.tv__subtitle2));
    UiUtils.show(mRootView.findViewById(R.id.user_agreement_block));

    initAgreementBlock(mRootView, savedInstanceState);
    initContinueBtn();
    return dialog;
  }

  private void initContinueBtn()
  {
    View continueBtn = mRootView.findViewById(R.id.btn__continue);
    continueBtn.setOnClickListener(this);
    continueBtn.setEnabled(mPrivacyPolicyCheckbox.isChecked() && mTermsCheckbox.isChecked());
  }

  private void initAgreementBlock(@NonNull View contentView, @Nullable Bundle savedInstanceState)
  {
    initCheckedViews(contentView, savedInstanceState);
    initAgreementMessages(contentView);
  }

  private void initAgreementMessages(View contentView)
  {
    initAgreement(contentView,
                  R.id.term_of_use_welcome,
                  R.string.sign_agree_tof_gdpr,
                  Framework.nativeGetTermsOfUseLink());
    initAgreement(contentView,
                  R.id.privacy_policy_welcome ,
                  R.string.sign_agree_pp_gdpr ,
                  Framework.nativeGetPrivacyPolicyLink());
  }

  private void initAgreement(@NonNull View contentView,
                             @IdRes int viewId,
                             @StringRes int templateResId,
                             @NonNull String src)
  {
    Resources res = getContext().getResources();
    TextView agreementMessage = contentView.findViewById(viewId);
    Spanned agreementText = Html.fromHtml(res.getString(templateResId, src));
    agreementMessage.setText(agreementText);
    agreementMessage.setMovementMethod(LinkMovementMethod.getInstance());
  }

  private void initCheckedViews(@NonNull View contentView, @Nullable Bundle savedInstanceState)
  {
    mTermsCheckbox = contentView.findViewById(R.id.term_of_use_welcome_checkbox);
    mPrivacyPolicyCheckbox = contentView.findViewById(R.id.privacy_policy_welcome_checkbox);

    if (savedInstanceState == null)
    {
      mPrivacyPolicyCheckbox.setChecked(isPrivacyPolicyConfirm());
      mTermsCheckbox.setChecked(isTermOfUseConfirm());
    }

    mTermsCheckbox.setOnCheckedChangeListener(
        (buttonView, isChecked) -> onTermsCheckedChanged(isChecked));
    mPrivacyPolicyCheckbox.setOnCheckedChangeListener(
        (buttonView, isChecked) -> onPrivacyPolicyCheckedChanged(isChecked));
  }

  private void onPrivacyPolicyCheckedChanged(boolean isChecked)
  {
    onCheckedChanged(isChecked, mTermsCheckbox.isChecked());
  }

  private void onTermsCheckedChanged(boolean isChecked)
  {
    onCheckedChanged(isChecked, mPrivacyPolicyCheckbox.isChecked());
  }

  private void onCheckedChanged(boolean isChecked,
                                boolean isAnotherConditionChecked)

  {
    View continueBtn = mRootView.findViewById(R.id.btn__continue);
    continueBtn.setEnabled(isChecked && isAnotherConditionChecked);
  }

  @Override
  public void onClick(View v)
  {
    if (v.getId() != R.id.btn__continue)
      return;

    onContinueBtnClick();
  }

  private void onContinueBtnClick()
  {
    SharedPreferences.Editor editor = MwmApplication.prefs(getContext()).edit();
    editor
        .putBoolean(Constants.Prefs.USER_AGREEMENT_PRIVACY_POLICY, mPrivacyPolicyCheckbox.isChecked())
        .putBoolean(Constants.Prefs.USER_AGREEMENT_TERM_OF_USE, mTermsCheckbox.isChecked())
        .apply();
    if (mListener != null)
      mListener.onDialogDone();

    dismiss();
  }

  @StyleRes
  @Override
  protected int getFullscreenLightTheme()
  {
    return R.style.MwmTheme_DialogFragment_NoFullscreen;
  }

  @StyleRes
  @Override
  protected int getFullscreenDarkTheme()
  {
    return R.style.MwmTheme_DialogFragment_NoFullscreen_Night;
  }


  public static boolean isAgreementRefused(@NonNull Context context)
  {
    boolean isTermsConfirm = isTermOfUseConfirm(context);
    boolean isPolicyConfirm = isPrivacyPolicyConfirm(context);
    return !isTermsConfirm || !isPolicyConfirm;
  }

  private boolean isPrivacyPolicyConfirm()
  {
    return isPrivacyPolicyConfirm(getContext());
  }

  private boolean isTermOfUseConfirm()
  {
    return isTermOfUseConfirm(getContext());
  }

  private static boolean isPrivacyPolicyConfirm(Context context)
  {
    return isSingleAgreementOk(context, Constants.Prefs.USER_AGREEMENT_PRIVACY_POLICY);
  }

  private static boolean isTermOfUseConfirm(@NonNull Context context)
  {
    return isTermOfUseOk(context);
  }

  private static boolean isTermOfUseOk(@NonNull Context activity)
  {
    return isSingleAgreementOk(activity, Constants.Prefs.USER_AGREEMENT_TERM_OF_USE);
  }

  private static boolean isSingleAgreementOk(@NonNull Context activity, @NonNull String key)
  {
    SharedPreferences prefs = MwmApplication.prefs(activity);
    return prefs.getBoolean(key, false);
  }
}
