package com.mapswithme.maps.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Point;
import android.graphics.RectF;
import android.graphics.drawable.AnimationDrawable;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.support.v4.graphics.drawable.DrawableWrapper;
import android.util.AttributeSet;
import android.widget.ImageView;

import com.mapswithme.maps.R;
import com.mapswithme.util.Graphics;
import com.mapswithme.util.ThemeUtils;

/**
 * Draws progress wheel, consisting of circle with background and 'stop' button in the center of the circle.
 */
public class WheelProgressView extends ImageView
{
  private static final int DEFAULT_THICKNESS = 4;

  private int mProgress;
  private int mRadius;
  private Paint mFgPaint;
  private Paint mBgPaint;
  private int mStrokeWidth;
  private final RectF mProgressRect = new RectF(); // main rect for progress wheel
  private final RectF mCenterRect = new RectF(); // rect for stop button
  private final Point mCenter = new Point(); // center of rect
  private boolean mIsInit;
  private boolean mIsPending;
  private Drawable mCenterDrawable;
  private AnimationDrawable mPendingDrawable;

  public WheelProgressView(Context context)
  {
    super(context);
    init(null);
  }

  public WheelProgressView(Context context, AttributeSet attrs)
  {
    super(context, attrs);
    init(attrs);
  }

  public WheelProgressView(Context context, AttributeSet attrs, int defStyle)
  {
    super(context, attrs, defStyle);
    init(attrs);
  }

  private void init(AttributeSet attrs)
  {
    final TypedArray typedArray = getContext().obtainStyledAttributes(attrs, R.styleable.WheelProgressView, 0, 0);
    mStrokeWidth = typedArray.getDimensionPixelSize(R.styleable.WheelProgressView_wheelThickness, DEFAULT_THICKNESS);
    final int progressColor = typedArray.getColor(R.styleable.WheelProgressView_wheelProgressColor, Color.WHITE);
    final int secondaryColor = typedArray.getColor(R.styleable.WheelProgressView_wheelSecondaryColor, Color.GRAY);
    mCenterDrawable = typedArray.getDrawable(R.styleable.WheelProgressView_centerDrawable);
    if (mCenterDrawable == null)
      mCenterDrawable = Graphics.tint(getContext(), R.drawable.ic_close_spinner);
    typedArray.recycle();

    mPendingDrawable = (AnimationDrawable) getResources().getDrawable(ThemeUtils.getResource(getContext(), R.attr.wheelPendingAnimation));
    Graphics.tint(mPendingDrawable, progressColor);

    mBgPaint = new Paint();
    mBgPaint.setColor(secondaryColor);
    mBgPaint.setStrokeWidth(mStrokeWidth);
    mBgPaint.setStyle(Paint.Style.STROKE);
    mBgPaint.setAntiAlias(true);

    mFgPaint = new Paint();
    mFgPaint.setColor(progressColor);
    mFgPaint.setStrokeWidth(mStrokeWidth);
    mFgPaint.setStyle(Paint.Style.STROKE);
    mFgPaint.setAntiAlias(true);
  }

  public void setProgress(int progress)
  {
    mProgress = progress;
    invalidate();
  }

  public int getProgress()
  {
    return mProgress;
  }

  @Override
  protected void onSizeChanged(int w, int h, int oldw, int oldh)
  {
    final int left = getPaddingLeft();
    final int top = getPaddingTop();
    final int right = w - getPaddingRight();
    final int bottom = h - getPaddingBottom();
    final int width = right - left;
    final int height = bottom - top;

    mRadius = (Math.min(width, height) - mStrokeWidth) / 2;
    mCenter.set(left + width / 2, top + height / 2);
    mProgressRect.set(mCenter.x - mRadius, mCenter.y - mRadius, mCenter.x + mRadius, mCenter.y + mRadius);

    Drawable d = ((mCenterDrawable instanceof DrawableWrapper) ? ((DrawableWrapper)mCenterDrawable).getWrappedDrawable()
                                                               : mCenterDrawable);
    if (d instanceof BitmapDrawable)
    {
      Bitmap bmp = ((BitmapDrawable)d).getBitmap();
      int halfw = bmp.getWidth() / 2;
      int halfh = bmp.getHeight() / 2;
      mCenterDrawable.setBounds(mCenter.x - halfw, mCenter.y - halfh, mCenter.x + halfw, mCenter.y + halfh);
    }
    else
      mCenterRect.set(mProgressRect);

    mIsInit = true;
  }

  @Override
  protected void onDraw(Canvas canvas)
  {
    if (mIsInit)
    {
      super.onDraw(canvas);

      if (!mIsPending)
      {
        canvas.drawCircle(mCenter.x, mCenter.y, mRadius, mBgPaint);
        canvas.drawArc(mProgressRect, -90, 360 * mProgress / 100, false, mFgPaint);
      }

      mCenterDrawable.draw(canvas);
    }
  }

  public void setPending(boolean pending)
  {
    mIsPending = pending;
    if (mIsPending)
    {
      mPendingDrawable.start();
      setImageDrawable(mPendingDrawable);
    }
    else
    {
      setImageDrawable(null);
      mPendingDrawable.stop();
    }

    invalidate();
  }

  public boolean isPending()
  {
    return mIsPending;
  }
}
