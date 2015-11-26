package com.mapswithme.maps.settings;

import android.annotation.TargetApi;
import android.os.Build;
import android.support.annotation.WorkerThread;
import android.util.Log;

import com.mapswithme.maps.BuildConfig;
import com.mapswithme.maps.MwmApplication;
import com.mapswithme.util.Constants;
import com.mapswithme.util.Utils;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.FilenameFilter;
import java.io.IOException;
import java.nio.channels.FileChannel;
import java.util.ArrayList;
import java.util.List;

public final class StorageUtils
{
  private StorageUtils() {}

  private static final int VOLD_MODE = 1;
  private static final int MOUNTS_MODE = 2;

  /**
   * Check if directory is writable. On some devices with KitKat (eg, Samsung S4) simple File.canWrite() returns
   * true for some actually read only directories on sdcard.
   * see https://code.google.com/p/android/issues/detail?id=66369 for details
   *
   * @param path path to ckeck
   * @return result
   */
  @SuppressWarnings("ResultOfMethodCallIgnored")
  public static boolean isPathWritable(String path)
  {
    final File f = new File(path, "testDir");
    f.mkdir();
    if (f.exists())
    {
      f.delete();
      return true;
    }

    return false;
  }

  public static long getFreeBytesAtPath(String path)
  {
    long size = 0;
    try
    {
      size = new File(path).getFreeSpace();
    } catch (RuntimeException e)
    {
      e.printStackTrace();
    }

    return size;
  }

  static List<String> parseStorages()
  {
    final List<String> results = new ArrayList<>();
    if (Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.KITKAT)
      results.addAll(parseKitkatStorages());

    results.addAll(parseConfigStorages());
    return results;
  }

  static List<String> parseConfigStorages()
  {
    final List<String> storages = new ArrayList<>();
    storages.addAll(parseMountFile("/etc/vold.conf", VOLD_MODE));
    storages.addAll(parseMountFile("/etc/vold.fstab", VOLD_MODE));
    storages.addAll(parseMountFile("/system/etc/vold.fstab", VOLD_MODE));
    storages.addAll(parseMountFile("/proc/mounts", MOUNTS_MODE));

    final List<String> results = new ArrayList<>();
    final String suffix = String.format(Constants.STORAGE_PATH, BuildConfig.APPLICATION_ID, Constants.FILES_DIR);
    for (String storage : storages)
    {
      Log.i(StoragePathManager.TAG, "Test storage from config files : " + storage);
      if (isPathWritable(storage))
      {
        Log.i(StoragePathManager.TAG, "Found writable storage : " + storage);
        results.add(storage);
      }
      else
      {
        storage += suffix;
        File file = new File(storage);
        if (!file.exists()) // create directory for our package if it isn't created by any reason
        {
          Log.i(StoragePathManager.TAG, "Try to create MWM path");
          file.mkdirs();
          file = new File(storage);
          if (file.exists())
            Log.i(StoragePathManager.TAG, "Created!");
        }
        if (isPathWritable(storage))
        {
          Log.i(StoragePathManager.TAG, "Found writable storage : " + storage);
          results.add(storage);
        }
      }
    }
    return results;
  }

  @TargetApi(Build.VERSION_CODES.KITKAT)
  @SuppressWarnings("ResultOfMethodCallIgnored")
  static List<String> parseKitkatStorages()
  {
    final List<String> results = new ArrayList<>();
    final File primaryStorage = MwmApplication.get().getExternalFilesDir(null);
    final File[] storages = MwmApplication.get().getExternalFilesDirs(null);
    if (storages != null)
    {
      for (File storage : storages)
      {
        if (storage != null && !storage.equals(primaryStorage))
        {
          Log.i(StoragePathManager.TAG, "Additional storage path: " + storage.getPath());
          results.add(storage.getPath());
        }
      }
    }

    return results;
  }

  // http://stackoverflow.com/questions/8151779/find-sd-card-volume-label-on-android
  // http://stackoverflow.com/questions/5694933/find-an-external-sd-card-location
  // http://stackoverflow.com/questions/14212969/file-canwrite-returns-false-on-some-devices-although-write-external-storage-pe
  static List<String> parseMountFile(String file, int mode)
  {
    final List<String> results = new ArrayList<>();
    Log.i(StoragePathManager.TAG, "Parsing " + file);

    BufferedReader reader = null;
    try
    {
      reader = new BufferedReader(new FileReader(file));

      while (true)
      {
        final String line = reader.readLine();
        if (line == null)
          break;

        // standard regexp for all possible whitespaces (space, tab, etc)
        final String[] arr = line.split("\\s+");

        // split may return empty first strings
        int start = 0;
        while (start < arr.length && arr[start].length() == 0)
          ++start;

        if (arr.length - start <= 3)
          continue;

        if (arr[start].charAt(0) == '#')
          continue;

        if (mode == VOLD_MODE)
        {
          if (arr[start].startsWith("dev_mount"))
            results.add(arr[start + 2]);
        }
        else
        {
          for (final String s : new String[]{"tmpfs", "/dev/block/vold", "/dev/fuse", "/mnt/media_rw"})
            if (arr[start].startsWith(s))
              results.add(arr[start + 1]);
        }
      }
    } catch (final IOException e)
    {
      Log.w(StoragePathManager.TAG, "Can't read file: " + file);
    } finally
    {
      Utils.closeStream(reader);
    }

    return results;
  }

  public static void copyFile(File source, File dest) throws IOException
  {
    int maxChunkSize = 10 * Constants.MB; // move file by smaller chunks to avoid OOM.
    FileChannel inputChannel = null, outputChannel = null;
    try
    {
      inputChannel = new FileInputStream(source).getChannel();
      outputChannel = new FileOutputStream(dest).getChannel();
      long totalSize = inputChannel.size();

      for (long currentPosition = 0; currentPosition < totalSize; currentPosition += maxChunkSize)
      {
        outputChannel.position(currentPosition);
        outputChannel.transferFrom(inputChannel, currentPosition, maxChunkSize);
      }
    } finally
    {
      if (inputChannel != null)
        inputChannel.close();
      if (outputChannel != null)
        outputChannel.close();
    }
  }

  public static long getDirSizeRecursively(File file, FilenameFilter fileFilter)
  {
    if (file.isDirectory())
    {
      long dirSize = 0;
      for (File child : file.listFiles())
        dirSize += getDirSizeRecursively(child, fileFilter);

      return dirSize;
    }

    if (fileFilter.accept(file.getParentFile(), file.getName()))
      return file.length();

    return 0;
  }

  /**
   * Recursively filters and lists all files in the directory.
   */
  public static void listFilesRecursively(File dir, String prefix, FilenameFilter filter, List<String> relPaths)
  {
    for (File file : dir.listFiles())
    {
      if (file.isDirectory())
      {
        listFilesRecursively(file, prefix + file.getName() + File.separator, filter, relPaths);
        continue;
      }
      String name = file.getName();
      if (filter.accept(dir, name))
        relPaths.add(prefix + name);
    }
  }

  @SuppressWarnings("ResultOfMethodCallIgnored")
  @WorkerThread
  public static void removeEmptyDirectories(File dir)
  {
    for (File file : dir.listFiles())
    {
      if (!file.isDirectory())
        continue;
      removeEmptyDirectories(file);
      file.delete();
    }
  }

  @SuppressWarnings("ResultOfMethodCallIgnored")
  @WorkerThread
  public static boolean removeFilesInDirectory(File dir, File[] files)
  {
    try
    {
      for (File file : files)
      {
        if (file != null)
          file.delete();
      }
      removeEmptyDirectories(dir);
      return true;
    } catch (Exception e)
    {
      e.printStackTrace();
      return false;
    }
  }

  @SuppressWarnings("ResultOfMethodCallIgnored")
  @WorkerThread
  public static String moveFiles(final File dstFile, final File srcFile, final FilenameFilter filter)
  {
    if (!dstFile.exists())
      dstFile.mkdir();

    final List<String> movablePaths = new ArrayList<>();
    StorageUtils.listFilesRecursively(srcFile, "", filter, movablePaths);

    final File[] oldFiles = new File[movablePaths.size()];
    final File[] newFiles = new File[movablePaths.size()];
    for (int i = 0; i < movablePaths.size(); ++i)
    {
      oldFiles[i] = new File(srcFile.getAbsolutePath(), movablePaths.get(i));
      newFiles[i] = new File(dstFile.getAbsolutePath(), movablePaths.get(i));
    }

    try
    {
      for (int i = 0; i < oldFiles.length; ++i)
      {
        final File parent = newFiles[i].getParentFile();
        if (parent != null)
          parent.mkdirs();
        StorageUtils.copyFile(oldFiles[i], newFiles[i]);
      }
    } catch (IOException e)
    {
      e.printStackTrace();
      // Error, remove partly copied files.
      StorageUtils.removeFilesInDirectory(dstFile, newFiles);
      return "IOException";
    }

    // Old files were copied, delete them.
    StorageUtils.removeFilesInDirectory(srcFile, oldFiles);
    return null;
  }
}
