package me.maps.utils;

import java.io.File;
import java.io.FileReader;
import java.util.Properties;


public class ConfigSingleton {

  private static final ConfigSingleton INSTANCE = new ConfigSingleton();
  private Properties props;


  private ConfigSingleton() {
    try {
      props = new Properties();
      props.load(new FileReader(new File("config.properties")));
    } catch (Exception e) {
      e.printStackTrace();
    }
  }


  public static ConfigSingleton getInstance() {
    return INSTANCE;
  }


  public String get(String key) {
    return props.getProperty(key);
  }


  public static String readConfig(String key) {
    return getInstance().get(key);
  }

}
