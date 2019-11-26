package com.mapswithme.maps.news;

import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;
import com.mapswithme.maps.R;
import com.mapswithme.util.UiUtils;

public enum OnboardingStep
{
  CHECK_OUT_SIGHTS(R.string.new_onboarding_step5_3_button,
                   R.string.later,
                   R.string.new_onboarding_step5_1_header,
                   R.string.new_onboarding_step5_3_message,
                   R.drawable.img_check_sights_out),
  SUBSCRIBE_TO_CATALOG(R.string.new_onboarding_step5_2_button,
                       R.string.later,
                       R.string.new_onboarding_step5_1_header,
                       R.string.new_onboarding_step5_2_message,
                       R.drawable.img_discover_guides),
  DISCOVER_GUIDES(R.string.new_onboarding_step5_1_button,
                  R.string.later,
                  R.string.new_onboarding_step5_1_header,
                  R.string.new_onboarding_step5_1_message,
                  R.drawable.img_discover_guides),
  SHARE_EMOTIONS(R.string.new_onboarding_button_2,
                 UiUtils.NO_ID,
                 R.string.new_onboarding_step4_header,
                 R.string.new_onboarding_step4_message,
                 R.drawable.img_share_emptions, false),
  EXPERIENCE(R.string.new_onboarding_button,
             UiUtils.NO_ID,
             R.string.new_onboarding_step3_header,
             R.string.new_onboarding_step3_message,
             R.drawable.img_experience, false),
  DREAM_AND_PLAN(R.string.new_onboarding_button,
                 UiUtils.NO_ID,
                 R.string.new_onboarding_step2_header,
                 R.string.new_onboarding_step2_message,
                 R.drawable.img_dream_and_plan, false),
  PERMISSION_EXPLANATION(R.string.new_onboarding_button,
                         R.string.learn_more,
                         R.string.onboarding_permissions_title,
                         R.string.onboarding_permissions_message,
                         R.drawable.img_welcome);

  @StringRes
  private final int mAcceptButtonResId;
  @StringRes
  private final int mDeclineButtonResId;
  @StringRes
  private final int mTitle;
  @StringRes
  private final int mSubtitle;
  @DrawableRes
  private final int mImage;

  private final boolean mDeclinedButton;

  OnboardingStep(@StringRes int acceptButtonResId, @StringRes int declineButtonResId,
                 @StringRes int title, @StringRes int subtitle, @DrawableRes int image)
  {

    this(acceptButtonResId, declineButtonResId, title, subtitle, image, true);
  }

  OnboardingStep(@StringRes int acceptButtonResId, @StringRes int declineButtonResId,
                 @StringRes int title, @StringRes int subtitle, @DrawableRes int image,
                 boolean hasDeclinedButton)
  {
    mAcceptButtonResId = acceptButtonResId;
    mDeclineButtonResId = declineButtonResId;
    mTitle = title;
    mSubtitle = subtitle;
    mImage = image;
    mDeclinedButton = hasDeclinedButton;
  }


  @StringRes
  public int getAcceptButtonResId()
  {
    return mAcceptButtonResId;
  }

  @StringRes
  public int getTitle()
  {
    return mTitle;
  }

  @StringRes
  public int getSubtitle()
  {
    return mSubtitle;
  }

  @DrawableRes
  public int getImage()
  {
    return mImage;
  }

  public boolean hasDeclinedButton()
  {
    return mDeclinedButton;
  }

  @StringRes
  public int getDeclinedButtonResId()
  {
    if (!hasDeclinedButton())
      throw new UnsupportedOperationException("Value : " + name());

    return mDeclineButtonResId;
  }
}
