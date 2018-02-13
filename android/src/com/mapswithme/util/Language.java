package com.mapswithme.util;

import android.content.Context;
import android.support.annotation.NonNull;
import android.text.TextUtils;
import android.view.inputmethod.InputMethodManager;
import android.view.inputmethod.InputMethodSubtype;

import com.mapswithme.maps.MwmApplication;

import java.util.Locale;

public class Language
{
  // Locale.getLanguage() returns even 3-letter codes, not that we need in the C++ core,
  // so we use locale itself, like zh_CN
  @NonNull
  public static String getDefaultLocale()
  {
    String lang = Locale.getDefault().toString();
    if (TextUtils.isEmpty(lang))
      return Locale.US.toString();

    // Replace deprecated language codes:
    // in* -> id
    // iw* -> he
    if (lang.startsWith("in"))
      return "id";
    if (lang.startsWith("iw"))
      return "he";

    return lang;
  }

  // After some testing on Galaxy S4, looks like this method doesn't work on all devices:
  // sometime it always returns the same value as getDefaultLocale()
  @NonNull
  public static String getKeyboardLocale()
  {
    Context context = MwmApplication.get();
    InputMethodManager imm = (InputMethodManager) context
        .getSystemService(Context.INPUT_METHOD_SERVICE);
    if (imm == null)
      return getDefaultLocale();

    final InputMethodSubtype ims = imm.getCurrentInputMethodSubtype();
    if (ims == null)
      return getDefaultLocale();

    String locale = ims.getLocale();
    if (TextUtils.isEmpty(locale.trim()))
      return getDefaultLocale();

    return locale;
  }

  @NonNull
  public static native String nativeNormalize(@NonNull String locale);
}
