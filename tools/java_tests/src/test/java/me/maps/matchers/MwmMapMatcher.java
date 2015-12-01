package me.maps.matchers;

import static me.maps.utils.Utils.VERSION;

import java.util.Map;
import java.util.Set;
import java.util.TreeSet;

import org.hamcrest.Description;
import org.hamcrest.TypeSafeMatcher;


public class MwmMapMatcher extends TypeSafeMatcher<Map<String, Long>> {

  private Map<String, Long> oldValues;
  private Map<String, Long> newValues;

  private double allowedRange = 0;

  private StringBuffer mismatchDescr = new StringBuffer();


  private MwmMapMatcher(Map<String, Long> oldValues) {
    this.oldValues = oldValues;
  }


  public static MwmMapMatcher matchesMwmSizes(Map<String, Long> oldValues) {
    return new MwmMapMatcher(oldValues);
  }


  public MwmMapMatcher withinPercent(double percent) {
    this.allowedRange = percent;
    return this;
  }


  private String version(Map<String, Long> map) {
    if (map == null || map.get(VERSION) == null) {
      return "Unknown";
    }
    return String.valueOf(map.get(VERSION));
  }


  @Override
  public void describeTo(Description description) {
    description.appendText(String.format("\n\nComparing:\n\tOldVersion: %s\n\tNewVersion: %s\n", version(oldValues), version(newValues)));
    description.appendText(String.format("All MWM sizes must be within %s%% of the old value.\n", allowedRange));
  }


  @Override
  protected boolean matchesSafely(Map<String, Long> map) {
    newValues = map;

    boolean passed = true;

    Set<String> newKeys = new TreeSet<String>(newValues.keySet());
    Set<String> oldKeys = new TreeSet<String>(oldValues.keySet());
    Set<String> intersection = new TreeSet<String>(newValues.keySet());

    boolean keysAreDifferent = intersection.retainAll(oldKeys);
    if (keysAreDifferent) {
      passed = false;
    }

    for (String key : intersection) {

      long oldSize = oldValues.get(key);
      long newSize = newValues.get(key);

      double percentage = Math.abs((newSize - oldSize) / (oldSize / 100.0));
      boolean locComparison = oldSize == newSize || percentage <= allowedRange;
      // we compare the actual sized to avoid NaN when both sizes are 0

      if (!locComparison) {
        mismatchDescr.append(String.format("\n%s\nOld size: %d, New size: %d; ", key, oldSize, newSize));
        if (newSize > oldSize) {
          mismatchDescr.append(String.format(" new size is bigger by %d bytes (", newSize - oldSize));
        } else {
          mismatchDescr.append(String.format(" new size is smaller by %d bytes (", oldSize - newSize));
        }
        mismatchDescr.append(String.format("%s%%)\n", percentage));
      }
      passed &= locComparison;
    }

    if (keysAreDifferent) {
      oldKeys.removeAll(intersection);
      newKeys.removeAll(intersection);

      addKeysToDescription(oldKeys, mismatchDescr, "Old");
      addKeysToDescription(newKeys, mismatchDescr, "New");
    }
    return passed;
  }


  private void addKeysToDescription(Set<String> keys, StringBuffer desc, String prefix) {
    desc.append(String.format("\n%s file contains the following extra keys:\n", prefix));
    for (String k : keys) {
      desc.append(String.format("%s\n", k));
    }
  }


  @Override
  protected void describeMismatchSafely(Map<String, Long> item, Description mismatchDescription) {
    mismatchDescription.appendText(mismatchDescr.toString());
  }
}