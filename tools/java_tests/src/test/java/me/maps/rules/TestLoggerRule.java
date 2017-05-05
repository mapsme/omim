package me.maps.rules;

import org.junit.rules.TestWatcher;
import org.junit.runner.Description;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;


public class TestLoggerRule extends TestWatcher {

  private static Logger log = LoggerFactory.getLogger(TestLoggerRule.class);


  @Override
  public void starting(Description d) {
    log.trace(String.format("\n\nStarting : %s.%s", d.getClassName(), d.getMethodName()));
  }


  @Override
  public void finished(Description d) {
    log.trace(String.format("Finished : %s.%s", d.getClassName(), d.getMethodName()));
  }
}
