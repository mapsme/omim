package com.mapswithme.maps.downloader;

import java.lang.ref.WeakReference;
import java.util.Timer;
import java.util.TimerTask;

import android.annotation.SuppressLint;
import android.app.DownloadManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.database.Cursor;
import android.net.Uri;
import android.os.Handler;
import android.os.Message;


@SuppressLint("NewApi")
public class Downloader
{
  private Context m_context;
  private DownloadManager m_manager;

  private static final int PROGRESS_PERIOD_MS = 2000;
  private Timer m_progress = null;

  private long m_current = -1;
  private long m_callback = -1;
  private boolean isActive()
  {
    return (m_current != -1 && m_callback != -1);
  }

  private native void onProgress(long size, long callback);
  private native void onFinish(int httpCode, long callback);

  private void onProgress()
  {
    if (!isActive())
      return;

    final Cursor c = m_manager.query(new DownloadManager.Query().setFilterById(m_current));
    if (c != null && c.moveToFirst())
      onProgress(c.getLong(c.getColumnIndex(DownloadManager.COLUMN_BYTES_DOWNLOADED_SO_FAR)), m_callback);
  }

  private void onFinish()
  {
    if (!isActive())
      return;

    final Cursor c = m_manager.query(new DownloadManager.Query().setFilterById(m_current));
    if (c != null && c.moveToFirst())
    {
      int httpCode = -1;
      switch (c.getInt(c.getColumnIndex(DownloadManager.COLUMN_STATUS)))
      {
      case DownloadManager.STATUS_SUCCESSFUL:
        // set OK code and reset current download
        httpCode = 200;
        m_current = -1;
        break;

      case DownloadManager.STATUS_FAILED:
        // pass HTTP error code or internal error code (DownloadManager.ERROR_xxx)
        httpCode = c.getInt(c.getColumnIndex(DownloadManager.COLUMN_REASON));
        break;

      case DownloadManager.STATUS_PAUSED:
        break;
      case DownloadManager.STATUS_PENDING:
        break;
      case DownloadManager.STATUS_RUNNING:
        break;
      default:
        break;
      }

      if (httpCode != -1)
      {
        removeCurrent();

        onFinish(httpCode, m_callback);
      }
    }
  }

  private void removeCurrent()
  {
    if (m_progress != null)
    {
      m_progress.cancel();
      m_progress = null;
    }

    if (m_current != -1)
    {
      m_manager.remove(m_current);
      m_current = -1;
    }
  }

  private BroadcastReceiver m_onComplete = new BroadcastReceiver()
  {
    @Override
    public void onReceive(Context ctxt, Intent intent)
    {
      onFinish();
    }
  };

  // This stuff with WeakReference is needed for the reason, described here:
  // http://stackoverflow.com/questions/11407943/this-handler-class-should-be-static-or-leaks-might-occur-incominghandler
  private static class NestedHandler extends Handler
  {
    private WeakReference<Downloader> m_parent;

    public void setReference(Downloader parent)
    {
      m_parent = new WeakReference<Downloader>(parent);
    }

    @Override
    public void handleMessage(Message msg)
    {
      Downloader p = m_parent.get();
      if (p != null)
        p.onProgress();
    }
  }

  private static NestedHandler m_handler = null;

  public Downloader(Context context)
  {
    m_context = context;
    m_manager = (DownloadManager) context.getSystemService(Context.DOWNLOAD_SERVICE);

    context.registerReceiver(m_onComplete, new IntentFilter(DownloadManager.ACTION_DOWNLOAD_COMPLETE));

    // create Handler to process progress timer notifications in UI thread
    if (m_handler == null)
      m_handler = new NestedHandler();
    m_handler.setReference(this);
  }

  public void startDownload(String title, String url, String filePath, long callback)
  {
    removeCurrent();

    m_callback = callback;

    m_current = m_manager.enqueue(new DownloadManager.Request(Uri.parse(url))
    .setAllowedNetworkTypes(DownloadManager.Request.NETWORK_WIFI | DownloadManager.Request.NETWORK_MOBILE)
    .setTitle(title)
    .setDestinationUri(Uri.parse("file://" + filePath)));

    // create timer to notify UI about download progress
    m_progress = new Timer();
    m_progress.schedule(new TimerTask()
    {
      @Override
      public void run()
      {
        // send update message in UI thread
        m_handler.sendMessage(m_handler.obtainMessage(1));
      }
    }, PROGRESS_PERIOD_MS, PROGRESS_PERIOD_MS);
  }

  public void cancelDownload()
  {
    m_callback = -1;

    removeCurrent();
  }
}
