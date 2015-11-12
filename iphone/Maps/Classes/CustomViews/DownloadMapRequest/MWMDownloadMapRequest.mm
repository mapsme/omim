#import "Common.h"
#import "LocationManager.h"
#import "MapsAppDelegate.h"
#import "MWMCircularProgress.h"
#import "MWMDownloadMapRequest.h"
#import "MWMDownloadMapRequestView.h"

#include "Framework.h"
#include "storage/index.hpp"

@interface MWMDownloadMapRequest () <MWMCircularProgressProtocol>

@property (nonatomic) IBOutlet MWMDownloadMapRequestView * rootView;
@property (nonatomic) IBOutlet UILabel * mapTitleLabel;
@property (nonatomic) IBOutlet UIButton * downloadMapButton;
@property (nonatomic) IBOutlet UIButton * downloadRoutesButton;
@property (nonatomic) IBOutlet UIView * progressViewWrapper;

@property (nonatomic) MWMCircularProgress * progressView;

@property (copy, nonatomic) NSString * mapSize;
@property (copy, nonatomic) NSString * mapAndRouteSize;

@property (weak, nonatomic) id <MWMDownloadMapRequestDelegate> delegate;

@property (nonatomic) storage::TIndex currentCountryIndex;

@end

@implementation MWMDownloadMapRequest

- (nonnull instancetype)initWithParentView:(nonnull UIView *)parentView
                                  delegate:(nonnull id <MWMDownloadMapRequestDelegate>)delegate
{
  self = [super init];
  if (self)
  {
    [[NSBundle mainBundle] loadNibNamed:self.class.className owner:self options:nil];
    [parentView addSubview:self.rootView];
    self.progressView = [[MWMCircularProgress alloc] initWithParentView:self.progressViewWrapper];
    self.progressView.delegate = self;
    [self setupProgressImages];
    self.delegate = delegate;
    [self showRequest];
  }
  return self;
}

- (void)setupProgressImages
{
  [self.progressView setImage:[UIImage imageNamed:@"ic_download"] forState:MWMCircularProgressStateNormal];
  [self.progressView setImage:[UIImage imageNamed:@"ic_download_press"] forState:MWMCircularProgressStateHighlighted];
  [self.progressView setImage:[UIImage imageNamed:@"ic_download"] forState:MWMCircularProgressStateSelected];
  [self.progressView setImage:[UIImage imageNamed:@"ic_download_press"] forState:MWMCircularProgressStateSelectedHighlighted];
  [self.progressView setImage:[UIImage imageNamed:@"ic_close_spinner"] forState:MWMCircularProgressStateProgress];
  [self.progressView setImage:[UIImage imageNamed:@"ic_close_spinner_press"] forState:MWMCircularProgressStateProgressHighlighted];
  [self.progressView setImage:[UIImage imageNamed:@"ic_download_error"] forState:MWMCircularProgressStateFailed];
  [self.progressView setImage:[UIImage imageNamed:@"ic_download_error_press"] forState:MWMCircularProgressStateFailedHighlighted];
  [self.progressView setImage:[UIImage imageNamed:@"ic_check"] forState:MWMCircularProgressStateCompleted];
}

- (void)dealloc
{
  [self.rootView removeFromSuperview];
}

- (void)showRequest
{
  auto & f = GetFramework();
  auto & activeMapLayout = f.GetCountryTree().GetActiveMapLayout();
  if (activeMapLayout.IsDownloadingActive())
  {
    self.currentCountryIndex = activeMapLayout.GetCurrentDownloadingCountryIndex();
    self.progressView.state = MWMCircularProgressStateProgress;
    [self updateState:MWMDownloadMapRequestStateDownload];
  }
  else
  {
    self.currentCountryIndex = storage::TIndex();
    double lat, lon;
    if ([[MapsAppDelegate theApp].m_locationManager getLat:lat Lon:lon])
      self.currentCountryIndex = f.GetCountryIndex(MercatorBounds::FromLatLon(lat, lon));

    if (self.currentCountryIndex.IsValid())
    {
      self.mapTitleLabel.text = @(activeMapLayout.GetFormatedCountryName(self.currentCountryIndex).c_str());
      LocalAndRemoteSizeT const sizes = activeMapLayout.GetRemoteCountrySizes(self.currentCountryIndex);
      self.mapSize = formattedSize(sizes.first);
      self.mapAndRouteSize = formattedSize(sizes.first + sizes.second);
      [self.downloadMapButton setTitle:[NSString stringWithFormat:@"%@ (%@)",
                                        L(@"downloader_download_map"), self.mapAndRouteSize]
                              forState:UIControlStateNormal];
      [self updateState:MWMDownloadMapRequestStateRequestLocation];
    }
    else
    {
      [self updateState:MWMDownloadMapRequestStateRequestUnknownLocation];
    }
  }
  self.rootView.hidden = NO;
}

#pragma mark - Process control

- (void)downloadProgress:(CGFloat)progress countryName:(nonnull NSString *)countryName
{
  self.progressView.progress = progress;
  [self showRequest];
  self.mapTitleLabel.text = countryName;
}

- (void)setDownloadFailed
{
  self.progressView.state = MWMCircularProgressStateFailed;
  [self.progressView stopSpinner];
}

#pragma mark - MWMCircularProgressDelegate

- (void)progressButtonPressed:(nonnull MWMCircularProgress *)progress
{
  auto & activeMapLayout = GetFramework().GetCountryTree().GetActiveMapLayout();
  if (progress.state == MWMCircularProgressStateFailed)
  {
    activeMapLayout.RetryDownloading(self.currentCountryIndex);
    [self.progressView startSpinner];
  }
  else
  {
    activeMapLayout.CancelDownloading(self.currentCountryIndex);
  }
  [self showRequest];
}

#pragma mark - Actions

- (IBAction)downloadMapTouchUpInside:(nonnull UIButton *)sender
{
  auto const mapType = self.downloadRoutesButton.selected ? MapOptions::MapWithCarRouting : MapOptions::Map;
  GetFramework().GetCountryTree().GetActiveMapLayout().DownloadMap(self.currentCountryIndex, mapType);
  self.progressView.progress = 0.0;
  [self showRequest];
  [self.progressView startSpinner];
}

- (IBAction)downloadRoutesTouchUpInside:(nonnull UIButton *)sender
{
  sender.selected = !sender.selected;
  [self.downloadMapButton setTitle:[NSString stringWithFormat:@"%@ (%@)",
                                    L(@"downloader_download_map"),
                                    sender.selected ? self.mapAndRouteSize : self.mapSize]
                          forState:UIControlStateNormal];
}

- (IBAction)selectMapTouchUpInside:(nonnull UIButton *)sender
{
  [self.delegate selectMapsAction];
}

- (void)updateState:(enum MWMDownloadMapRequestState)state
{
  self.rootView.state = state;
  [self.delegate stateUpdated:state];
}

@end
