#import "WebViewController.h"

@implementation WebViewController

- (id)initWithUrl:(NSURL *)url andTitleOrNil:(NSString *)title
{
  self = [super initWithNibName:nil bundle:nil];
  if (self)
  {
    self.m_url = url;
    if (title)
      self.navigationItem.title = title;
  }
  return self;
}

- (id)initWithHtml:(NSString *)htmlText baseUrl:(NSURL *)url andTitleOrNil:(NSString *)title
{
  self = [super initWithNibName:nil bundle:nil];
  if (self)
  {
    htmlText = [htmlText stringByReplacingOccurrencesOfString:@"<body>" withString:@"<body><font face=\"helvetica\">"];
    htmlText = [htmlText stringByReplacingOccurrencesOfString:@"</body>" withString:@"</font></body>"];
    self.m_htmlText = htmlText;
    self.m_url = url;
    if (title)
      self.navigationItem.title = title;
  }
  return self;
}

- (void)loadView
{
  CGRect frame = [UIScreen mainScreen].applicationFrame;
  UIWebView * webView = [[UIWebView alloc] initWithFrame:frame];
  webView.autoresizesSubviews = YES;
  webView.autoresizingMask = (UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleWidth);
  webView.backgroundColor = [UIColor whiteColor];
  webView.delegate = self;

  if (self.m_htmlText)
    [webView loadHTMLString:self.m_htmlText baseURL:self.m_url];
  else
    [webView loadRequest:[NSURLRequest requestWithURL:self.m_url]];

  self.view = webView;
}

- (BOOL)webView:(UIWebView *)inWeb shouldStartLoadWithRequest:(NSURLRequest *)inRequest navigationType:(UIWebViewNavigationType)inType
{
  if (self.openInSafari && inType == UIWebViewNavigationTypeLinkClicked
      && ![inRequest.URL.scheme isEqualToString:@"applewebdata"]) // do not try to open local links in Safari
  {
    [[UIApplication sharedApplication] openURL:[inRequest URL]];
    return NO;
  }

  return YES;
}

@end
