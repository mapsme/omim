package com.mapswithme.util;

import android.content.Context;
import android.view.inputmethod.InputMethodManager;
import android.view.inputmethod.InputMethodSubtype;
import com.mapswithme.maps.MwmApplication;

import java.util.Locale;

public class Language
{
  // Locale.getLanguage() returns even 3-letter codes, not that we need in the C++ core,
  // so we use locale itself, like zh_CN
  public static String getDefaultLocale()
  {
    return Locale.getDefault().toString();
  }

  // After some testing on Galaxy S4, looks like this method doesn't work on all devices:
  // sometime it always returns the same value as getDefaultLocale()
  public static String getKeyboardLocale()
  {
    final InputMethodManager imm = (InputMethodManager) MwmApplication.get().getSystemService(Context.INPUT_METHOD_SERVICE);
    if (imm != null)
    {
      final InputMethodSubtype ims = imm.getCurrentInputMethodSubtype();
      if (ims != null)
        return ims.getLocale();
    }

    return getDefaultLocale();
  }
}
