package com.mapswithme.maps.downloader;

import android.os.AsyncTask;
import android.util.Base64;

import java.io.BufferedInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.Map;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

import com.mapswithme.util.Constants;
import com.mapswithme.util.StringUtils;
import com.mapswithme.util.Utils;
import com.mapswithme.util.log.Logger;
import com.mapswithme.util.log.LoggerFactory;

@SuppressWarnings("unused")
class ChunkTask extends AsyncTask<Void, byte[], Boolean>
{
  private static final Logger LOGGER = LoggerFactory.INSTANCE.getLogger(LoggerFactory.Type.DOWNLOADER);
  private static final String TAG = "ChunkTask";

  private static final int TIMEOUT_IN_SECONDS = 60;

  private final long mHttpCallbackID;
  private final String mUrl;
  private final long mBeg;
  private final long mEnd;
  private final long mExpectedFileSize;
  private byte[] mPostBody;
  private final String mUserAgent;

  private static final int NOT_SET = -1;
  private static final int IO_ERROR = -2;
  private static final int INVALID_URL = -3;
  private static final int WRITE_ERROR = -4;
  private static final int FILE_SIZE_CHECK_FAILED = -5;

  private int mHttpErrorCode = NOT_SET;
  private long mDownloadedBytes;

  private static final Executor sExecutors = Executors.newFixedThreadPool(4);

  public ChunkTask(long httpCallbackID, String url, long beg, long end,
                   long expectedFileSize, byte[] postBody, String userAgent)
  {
    mHttpCallbackID = httpCallbackID;
    mUrl = url;
    mBeg = beg;
    mEnd = end;
    mExpectedFileSize = expectedFileSize;
    mPostBody = postBody;
    mUserAgent = userAgent;
  }

  @Override
  protected void onPreExecute() {}

  private long getChunkID()
  {
    return mBeg;
  }

  @Override
  protected void onPostExecute(Boolean success)
  {
    //Log.i(TAG, "Writing chunk " + getChunkID());

    // It seems like onPostExecute can be called (from GUI thread queue)
    // after the task was cancelled in destructor of HttpThread.
    // Reproduced by Samsung testers: touch Try Again for many times from
    // start activity when no connection is present.

    if (!isCancelled())
      nativeOnFinish(mHttpCallbackID, success ? 200 : mHttpErrorCode, mBeg, mEnd);
  }

  @Override
  protected void onProgressUpdate(byte[]... data)
  {
    if (!isCancelled())
    {
      // Use progress event to save downloaded bytes.
      if (nativeOnWrite(mHttpCallbackID, mBeg + mDownloadedBytes, data[0], data[0].length))
        mDownloadedBytes += data[0].length;
      else
      {
        // Cancel downloading and notify about error.
        cancel(false);
        nativeOnFinish(mHttpCallbackID, WRITE_ERROR, mBeg, mEnd);
      }
    }
  }

  void start()
  {
    executeOnExecutor(sExecutors, (Void[]) null);
  }

  private static long parseContentRange(String contentRangeValue)
  {
    if (contentRangeValue != null)
    {
      final int slashIndex = contentRangeValue.lastIndexOf('/');
      if (slashIndex >= 0)
      {
        try
        {
          return Long.parseLong(contentRangeValue.substring(slashIndex + 1));
        } catch (final NumberFormatException ex)
        {
          // Return -1 at the end of function
        }
      }
    }
    return -1;
  }

  @Override
  protected Boolean doInBackground(Void... p)
  {
    //Log.i(TAG, "Start downloading chunk " + getChunkID());

    HttpURLConnection urlConnection = null;
    /*
     * TODO improve reliability of connections & handle EOF errors.
     * <a href="http://stackoverflow.com/questions/19258518/android-httpurlconnection-eofexception">asd</a>
     */

    try
    {
      final URL url = new URL(mUrl);
      urlConnection = (HttpURLConnection) url.openConnection();

      if (isCancelled())
        return false;

      urlConnection.setUseCaches(false);
      urlConnection.setConnectTimeout(TIMEOUT_IN_SECONDS * 1000);
      urlConnection.setReadTimeout(TIMEOUT_IN_SECONDS * 1000);

      // Set user agent with unique client id
      urlConnection.setRequestProperty("User-Agent", mUserAgent);

      // Provide authorization credentials
      String creds = url.getUserInfo();
      if (creds != null)
      {
        String value = "Basic " + Base64.encodeToString(creds.getBytes(), Base64.DEFAULT);
        urlConnection.setRequestProperty("Authorization", value);
      }

      // use Range header only if we don't download whole file from start
      if (!(mBeg == 0 && mEnd < 0))
      {
        if (mEnd > 0)
          urlConnection.setRequestProperty("Range", StringUtils.formatUsingUsLocale("bytes=%d-%d", mBeg, mEnd));
        else
          urlConnection.setRequestProperty("Range", StringUtils.formatUsingUsLocale("bytes=%d-", mBeg));
      }

      final Map<?, ?> requestParams = urlConnection.getRequestProperties();

      if (mPostBody != null)
      {
        urlConnection.setDoOutput(true);
        urlConnection.setFixedLengthStreamingMode(mPostBody.length);

        final DataOutputStream os = new DataOutputStream(urlConnection.getOutputStream());
        os.write(mPostBody);
        os.flush();
        mPostBody = null;
        Utils.closeStream(os);
      }

      if (isCancelled())
        return false;

      final int err = urlConnection.getResponseCode();
      // @TODO We can handle redirect (301, 302 and 307) here and display redirected page to user,
      // to avoid situation when downloading is always failed by "unknown" reason
      // When we didn't ask for chunks, code should be 200
      // When we asked for a chunk, code should be 206
      final boolean isChunk = !(mBeg == 0 && mEnd < 0);
      if ((isChunk && err != HttpURLConnection.HTTP_PARTIAL) || (!isChunk && err != HttpURLConnection.HTTP_OK))
      {
        // we've set error code so client should be notified about the error
        mHttpErrorCode = FILE_SIZE_CHECK_FAILED;
        LOGGER.w(TAG, "Error for " + urlConnection.getURL() +
                   ": Server replied with code " + err +
                   ", aborting download. " + Utils.mapPrettyPrint(requestParams));
        return false;
      }

      // Check for content size - are we downloading requested file or some router's garbage?
      if (mExpectedFileSize > 0)
      {
        long contentLength = parseContentRange(urlConnection.getHeaderField("Content-Range"));
        if (contentLength < 0)
          contentLength = urlConnection.getContentLength();

        // Check even if contentLength is invalid (-1), in this case it's not our server!
        if (contentLength != mExpectedFileSize)
        {
          // we've set error code so client should be notified about the error
          mHttpErrorCode = FILE_SIZE_CHECK_FAILED;
          LOGGER.w(TAG, "Error for " + urlConnection.getURL() +
                     ": Invalid file size received (" + contentLength + ") while expecting " + mExpectedFileSize +
                     ". Aborting download.");
          return false;
        }
        // @TODO Else display received web page to user - router is redirecting us to some page
      }

      return downloadFromStream(new BufferedInputStream(urlConnection.getInputStream(), 65536));
    } catch (final MalformedURLException ex)
    {
      LOGGER.e(TAG, "Invalid url: " + mUrl, ex);

      mHttpErrorCode = INVALID_URL;
      return false;
    } catch (final IOException ex)
    {
      LOGGER.d(TAG, "IOException in doInBackground for URL: " + mUrl, ex);

      mHttpErrorCode = IO_ERROR;
      return false;
    } finally
    {
      if (urlConnection != null)
        urlConnection.disconnect();
      else
        mHttpErrorCode = IO_ERROR;
    }
  }

  private boolean downloadFromStream(InputStream stream)
  {
    // Because of timeouts in InputStream.read (for bad connection),
    // try to introduce dynamic buffer size to read in one query.
    final int arrSize[] = {64, 32, 1};
    int ret = -1;

    for (int size : arrSize)
    {
      try
      {
        ret = downloadFromStreamImpl(stream, size * Constants.KB);
        break;
      } catch (final IOException ex)
      {
        LOGGER.e(TAG, "IOException in downloadFromStream for chunk size: " + size, ex);
      }
    }

    if (ret < 0)
      mHttpErrorCode = IO_ERROR;

    Utils.closeStream(stream);

    return (ret == 0);
  }

  /**
   * @return 0 - download successful;
   * 1 - download canceled;
   * -1 - some error occurred;
   * @throws IOException
   */
  private int downloadFromStreamImpl(InputStream stream, int bufferSize) throws IOException
  {
    final byte[] tempBuf = new byte[bufferSize];

    int readBytes;
    while ((readBytes = stream.read(tempBuf)) > 0)
    {
      if (isCancelled())
        return 1;

      final byte[] chunk = new byte[readBytes];
      System.arraycopy(tempBuf, 0, chunk, 0, readBytes);

      publishProgress(chunk);
    }

    // -1 - means the end of the stream (success), else - some error occurred
    return (readBytes == -1 ? 0 : -1);
  }

  private static native boolean nativeOnWrite(long httpCallbackID, long beg, byte[] data, long size);
  private static native void nativeOnFinish(long httpCallbackID, long httpCode, long beg, long end);
}
