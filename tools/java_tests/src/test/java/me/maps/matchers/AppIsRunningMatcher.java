package me.maps.matchers;

import me.maps.utils.Adb;

import org.hamcrest.Description;
import org.hamcrest.TypeSafeMatcher;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;


public class AppIsRunningMatcher extends TypeSafeMatcher<String> {

  private static Logger log = LoggerFactory.getLogger(AppIsRunningMatcher.class);

  private long afterMillis = 0;
  private String appName = "";


  private AppIsRunningMatcher() {
  }


  public static AppIsRunningMatcher isRunning() {
    return new AppIsRunningMatcher();
  }


  public AppIsRunningMatcher afterSeconds(int seconds) {
    this.afterMillis = seconds * 1000;
    return this;
  }


  @Override
  public void describeTo(Description description) {
    description.appendText(String.format("App %s should be running after %d seconds", appName, afterMillis / 1000));
  }


  @Override
  protected boolean matchesSafely(String item) {
    this.appName = item;
    try {
      Thread.sleep(afterMillis);
      return Adb.appIsRunning(item);
    } catch (Exception e) {
      log.error("Failed during checking whether the app is running", e);
      return false;
    }
  }

}
