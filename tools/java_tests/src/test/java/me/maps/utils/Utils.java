package me.maps.utils;

import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;

import org.json.JSONArray;
import org.json.JSONObject;


public class Utils {
  public static final String VERSION = "__version__";


  public static Map<String, Long> readFilesizesFromCoutriesFile(String path) {
    try {
      List<String> lines = Files.readAllLines(Paths.get(path));
      String contents = lines.stream().collect(Collectors.joining());
      JSONObject json = new JSONObject(contents);
      return parseCountriesTxt(json);
    } catch (Exception e) {
      e.printStackTrace();
    }
    return null;
  }


  private static Map<String, Long> parseCountriesTxt(JSONObject json) throws Exception {
    Map<String, Long> ret = new HashMap<>();
    long version = 0;
    if (json.has("v")) {
      version = Long.parseLong(json.get("v").toString());
    }

    ret.put(VERSION, version);
    return parseJsonArray(json.getJSONArray("g"), ret);
  }


  private static Map<String, Long> parseJsonArray(JSONArray jarray, Map<String, Long> acc) throws Exception {
    for (Object ob : jarray) {
      JSONObject job = (JSONObject) ob;

      if (job.has("g")) {
        parseJsonArray(job.getJSONArray("g"), acc);
      } else {
        acc.put(job.getString("n"), job.getLong("s"));
        acc.put(job.getString("n") + ".routing", job.getLong("rs"));
      }
    }
    return acc;
  }
}
