package com.mapswithme.maps.promo;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import com.mapswithme.maps.Framework;
import com.mapswithme.maps.R;
import com.mapswithme.maps.base.BaseMwmDialogFragment;
import com.mapswithme.maps.bookmarks.ShowOnMapCatalogCategoryFragment;
import com.mapswithme.util.SharedPropertiesUtils;
import com.mapswithme.util.UiUtils;

public class NewTermsDialogFragment extends BaseMwmDialogFragment
{
  public static final String TAG = ShowOnMapCatalogCategoryFragment.class.getCanonicalName();

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container, @Nullable
      Bundle savedInstanceState)
  {
    View root = inflater.inflate(R.layout.fragment_new_terms, container, false);
    UiUtils.linkifyView(root, R.id.message, R.string.licence_agreement_popup_message,
                        Framework.nativeGetTermsOfUseLink(), Framework.nativeGetPrivacyPolicyLink(),
                        Framework.nativeGetTermsOfUseLink(), Framework.nativeGetPrivacyPolicyLink());
    View acceptBtn = root.findViewById(R.id.accept);
    acceptBtn.setOnClickListener(v -> onAccepted());
    View notNowBtn = root.findViewById(R.id.not_now);
    notNowBtn.setOnClickListener(v -> onDeclined());
    return root;
  }

  private void onDeclined()
  {
    dismissAllowingStateLoss();
  }

  private void onAccepted()
  {
    dismissAllowingStateLoss();
    SharedPropertiesUtils.setShouldShowNewTermsDialog(requireContext(), false);
  }

  public static void show(@NonNull FragmentManager fragmentManager)
  {
    Fragment fragment = fragmentManager.findFragmentByTag(NewTermsDialogFragment.class.getName());
    if (fragment != null)
      return;

    new NewTermsDialogFragment().show(fragmentManager, NewTermsDialogFragment.class.getName());
  }
}
