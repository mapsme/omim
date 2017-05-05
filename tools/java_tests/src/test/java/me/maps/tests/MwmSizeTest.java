package me.maps.tests;

import static me.maps.matchers.MwmMapMatcher.matchesMwmSizes;
import static me.maps.utils.ConfigSingleton.readConfig;
import static me.maps.utils.Utils.readFilesizesFromCoutriesFile;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;

import java.util.Map;

import me.maps.rules.TestLoggerRule;

import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;


public class MwmSizeTest {

  public static Map<String, Long> oldMwmSizes;
  public static Map<String, Long> newMwmSizes;

  @Rule
  public TestRule classRule = new TestLoggerRule();


  @BeforeClass
  public static void readMwmSizes() {
    oldMwmSizes = readFilesizesFromCoutriesFile(readConfig("oldMwmSizes"));
    newMwmSizes = readFilesizesFromCoutriesFile(readConfig("newMwmSizes"));
  }


  @Test
  public void test() {
    assertNotNull("Couldn't find countries.txt in the data folder", newMwmSizes);
    assertNotNull("Couldn't find old_countries.txt in the data folder", oldMwmSizes);
    assertThat("Some description in the method call", newMwmSizes, matchesMwmSizes(oldMwmSizes).withinPercent(10.0));
  }
}
