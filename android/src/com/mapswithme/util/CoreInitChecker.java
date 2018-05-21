package com.mapswithme.util;

import android.content.Context;
import android.support.annotation.NonNull;

public interface CoreInitChecker
{
  /**
   * Checks the core initialization and core dependency policy. According to the policy the check may
   * be successful or not.
   *
   * @return true if all conditions about core dependency are satisfied, otherwise - false.
   */
  boolean check(@NonNull Context context);

  /**
   * Indicates whether the caller requires the native core or not.
   *
   * @return true if the caller depends on the native core, otherwise - false.
   */
  boolean isCoreDependent();
}
