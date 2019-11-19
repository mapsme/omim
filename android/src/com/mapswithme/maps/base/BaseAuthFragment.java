package com.mapswithme.maps.base;

import android.content.Intent;
import androidx.annotation.CallSuper;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.mapswithme.maps.auth.Authorizer;
import com.mapswithme.maps.auth.TargetFragmentCallback;

public abstract class BaseAuthFragment extends BaseAsyncOperationFragment
    implements Authorizer.Callback, TargetFragmentCallback
{
  @NonNull
  private final Authorizer mAuthorizer = new Authorizer(this);

  protected void authorize()
  {
    mAuthorizer.authorize();
  }

  @Override
  @CallSuper
  public void onStart()
  {
    super.onStart();
    mAuthorizer.attach(this);
  }

  @Override
  @CallSuper
  public void onStop()
  {
    super.onStop();
    mAuthorizer.detach();
  }

  @Override
  public void onTargetFragmentResult(int resultCode, @Nullable Intent data)
  {
    mAuthorizer.onTargetFragmentResult(resultCode, data);
  }

  @Override
  public boolean isTargetAdded()
  {
    return isAdded();
  }

  protected boolean isAuthorized()
  {
    return mAuthorizer.isAuthorized();
  }
}
