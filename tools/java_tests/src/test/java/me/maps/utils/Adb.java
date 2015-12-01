package me.maps.utils;

import java.io.BufferedReader;
import java.io.File;
import java.io.InputStreamReader;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.stream.Collectors;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;


public class Adb {

  private static Logger log = LoggerFactory.getLogger(Adb.class);


  private Adb() {
  }


  public static void adbInstall(String pathToApk) throws Exception {
    adb(String.format("install %s", pathToApk));
  }


  private static String ls(String dir) throws Exception {
    return shell(String.format("ls %s", dir));
  }


  public static void startApp() throws Exception {
    startApp("com.mapswithme.maps.pro", "com.mapswithme.maps.DownloadResourcesActivity");
  }


  public static void startApp(String app, String activity) throws Exception {
    shell(String.format("am start -n %s/%s", app, activity));
  }


  public static void mkdir(String dir) throws Exception {
    shell(String.format("mkdir %s", dir));
  }


  public static void rmdir(String dir) throws Exception {
    shell(String.format("rm -r %s", dir));
  }


  public static void uninstall(String name) throws Exception {
    adb(String.format("uninstall %s", name));
  }


  private static void push(String what, String where) throws Exception {
    String filename = Paths.get(what).getFileName().toString();
    push(what, where, filename);
  }


  public static void push(String what, String where, String name) throws Exception {
    adb(String.format("push %s %s/%s", what, where, name));
  }


  public static boolean appIsRunning(String substring) throws Exception {
    String processes = shell(String.format("ps | grep %s", substring));
    log.debug(String.format("The processes are: %s", processes));
    return !processes.isEmpty();
  }


  private static List<String> devices() throws Exception {
    List<String> lines = Arrays.asList(adb("devices").split("\n"));
    return lines.stream().filter(s -> !s.startsWith("List of devices")).map(s -> {
      return s.split("\\s")[0];
    }).collect(Collectors.toList());
  }


  private static String shell(String command) throws Exception {
    String jointCommand = String.format("shell %s", command);
    return adb(jointCommand);
  }


  public static final List<String> packagesGrep(String substring) throws Exception {
    List<String> lines = Arrays.asList(shell("pm list packages -f").split("\n"));
    return lines.stream().filter(s -> s.contains(substring)).map(s -> {
      return s.split("apk=")[1];
    }).collect(Collectors.toList());
  }


  private static String adb(String command) throws Exception {

    List<String> cmdList = new ArrayList<>(Arrays.asList(command.split(" ")));
    cmdList.add(0, "adb");

    log.trace(cmdList.stream().collect(Collectors.joining(" ")));

    Process process = new ProcessBuilder(cmdList).redirectErrorStream(true).directory(new File(".")).start();

    StringBuffer sb = new StringBuffer();

    BufferedReader br = new BufferedReader(new InputStreamReader(process.getInputStream()));
    String line = null;
    while ((line = br.readLine()) != null) {
      sb.append(line).append("\n");
    }

    // There should really be a timeout here.
    if (0 != process.waitFor()) {
      return null;
    }

    return sb.toString();
  }

}
