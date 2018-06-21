package com.mapswithme.maps.editor;

import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.v7.widget.RecyclerView;

import com.mapswithme.maps.base.BaseMwmRecyclerFragment;
import com.mapswithme.maps.editor.data.Language;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashSet;
import java.util.List;
import java.util.Set;


public class LanguagesFragment extends BaseMwmRecyclerFragment<LanguagesAdapter>
{
  final static String EXISTING_LOCALIZED_NAMES = "ExistingLocalizedNames";

  public interface Listener
  {
    void onLanguageSelected(Language language);
  }

  @NonNull
  @Override
  protected LanguagesAdapter createAdapter()
  {
    Bundle args = getArguments();
    Set<String> existingLanguages = new HashSet<>(args.getStringArrayList(EXISTING_LOCALIZED_NAMES));

    List<Language> languages = new ArrayList<>();
    for (Language lang : Editor.nativeGetSupportedLanguages())
    {
      if (!existingLanguages.contains(lang.code))
        languages.add(lang);
    }

    Collections.sort(languages, new Comparator<Language>()
    {
      @Override
      public int compare(Language lhs, Language rhs) {
        // Default name can be changed, but it should be last in list of names.
        if (lhs.isDefaultLang() && !rhs.isDefaultLang())
          return 1;
        if (!lhs.isDefaultLang() && rhs.isDefaultLang())
          return -1;

        return lhs.name.compareTo(rhs.name);
      }
    });

    return new LanguagesAdapter(this, languages.toArray(new Language[languages.size()]));
  }

  protected void onLanguageSelected(Language language)
  {
    if (getParentFragment() instanceof Listener)
      ((Listener) getParentFragment()).onLanguageSelected(language);
  }
}
