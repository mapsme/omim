#import "MWMPurchaseManager.h"

#include "Framework.h"
#include "private.h"

#import <StoreKit/StoreKit.h>

@interface MWMPurchaseManager() <SKRequestDelegate>

@property(nonatomic, copy) ValidateReceiptCallback callback;
@property(nonatomic) SKReceiptRefreshRequest *receiptRequest;
@property(nonatomic, copy) NSString * serverId;
@property(nonatomic, copy) NSString * vendorId;

@end

@implementation MWMPurchaseManager

+ (NSString *)bookmarksSubscriptionServerId
{
  return @(BOOKMARKS_SUBSCRIPTION_SERVER_ID);
}

+ (NSString *)bookmarksSubscriptionVendorId
{
  return @(BOOKMARKS_SUBSCRIPTION_VENDOR);
}

+ (NSArray *)bookmakrsProductIds
{
  return @[@(BOOKMARKS_SUBSCRIPTION_YEARLY_PRODUCT_ID),
           @(BOOKMARKS_SUBSCRIPTION_MONTHLY_PRODUCT_ID)];
}

+ (NSString *)adsRemovalServerId
{
  return @(ADS_REMOVAL_SERVER_ID);
}

+ (NSString *)adsRemovalVendorId
{
  return @(ADS_REMOVAL_VENDOR);
}

+ (NSArray *)productIds
{
  return @[@(ADS_REMOVAL_YEARLY_PRODUCT_ID),
           @(ADS_REMOVAL_MONTHLY_PRODUCT_ID),
           @(ADS_REMOVAL_WEEKLY_PRODUCT_ID)];
}

+ (NSArray *)legacyProductIds
{
  auto pidVec = std::vector<std::string>(ADS_REMOVAL_NOT_USED_LIST);
  NSMutableArray *result = [NSMutableArray array];
  for (auto const & s : pidVec)
    [result addObject:@(s.c_str())];
  
  return [result copy];
}

+ (NSArray<NSString *> *)bookmarkInappIds
{
  auto pidVec = std::vector<std::string>(BOOKMARK_INAPP_IDS);
  NSMutableArray *result = [NSMutableArray array];
  for (auto const & s : pidVec)
    [result addObject:@(s.c_str())];

  return [result copy];
}

+ (MWMPurchaseManager *)sharedManager
{
  static MWMPurchaseManager *instance;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    instance = [[MWMPurchaseManager alloc] init];
  });
  return instance;
}

- (void)refreshReceipt
{
  self.receiptRequest = [[SKReceiptRefreshRequest alloc] init];
  self.receiptRequest.delegate = self;
  [self.receiptRequest start];
}

- (void)validateReceipt:(NSString *)serverId
               vendorId:(NSString *)vendorId
         refreshReceipt:(BOOL)refresh
               callback:(ValidateReceiptCallback)callback
{
  self.callback = callback;
  self.serverId = serverId;
  self.vendorId = vendorId;
  [self validateReceipt:refresh];
}

- (void)validateReceipt:(BOOL)refresh
{
  NSURL * receiptUrl = [NSBundle mainBundle].appStoreReceiptURL;
  NSData * receiptData = [NSData dataWithContentsOfURL:receiptUrl];

  if (!receiptData)
  {
    if (refresh)
      [self refreshReceipt];
    else
      [self noReceipt];
    return;
  }

  GetFramework().GetPurchase()->SetValidationCallback([self](auto validationCode, auto const & validationInfo)
  {
    switch (validationCode)
    {
    case Purchase::ValidationCode::Verified:
      [self validReceipt];
      break;
    case Purchase::ValidationCode::NotVerified:
      [self invalidReceipt];
      break;
    case Purchase::ValidationCode::ServerError:
    case Purchase::ValidationCode::AuthError:
      [self serverError];
      break;
    }
    GetFramework().GetPurchase()->SetValidationCallback(nullptr);
  });
  Purchase::ValidationInfo vi;
  vi.m_receiptData = [receiptData base64EncodedStringWithOptions:0].UTF8String;
  vi.m_serverId = self.serverId.UTF8String;
  vi.m_vendorId = self.vendorId.UTF8String;
  auto const accessToken = GetFramework().GetUser().GetAccessToken();
  GetFramework().GetPurchase()->Validate(vi, accessToken);
}

- (void)startTransaction:(NSString *)serverId callback:(StartTransactionCallback)callback {
  GetFramework().GetPurchase()->SetStartTransactionCallback([callback](bool success,
                                                                       std::string const & serverId,
                                                                       std::string const & vendorId)
  {
    callback(success, @(serverId.c_str()));
  });
  GetFramework().GetPurchase()->StartTransaction(serverId.UTF8String,
                                                 BOOKMARKS_VENDOR,
                                                 GetFramework().GetUser().GetAccessToken());
}

- (void)validReceipt
{
  if (self.callback)
    self.callback(self.serverId, MWMValidationResultValid);
}

- (void)noReceipt
{
  if (self.callback)
    self.callback(self.serverId, MWMValidationResultNotValid);
}

- (void)invalidReceipt
{
  if (self.callback)
    self.callback(self.serverId, MWMValidationResultNotValid);
}

- (void)serverError
{
  if (self.callback)
    self.callback(self.serverId, MWMValidationResultServerError);
}

- (void)authError
{
  if (self.callback)
    self.callback(self.serverId, MWMValidationResultAuthError);
}

- (void)appstoreError:(NSError *)error
{
  if (self.callback)
    self.callback(self.serverId, MWMValidationResultServerError);
}

- (void)setAdsDisabled:(BOOL)disabled
{
  GetFramework().GetPurchase()->SetSubscriptionEnabled(SubscriptionType::RemoveAds, disabled);
}

#pragma mark - SKRequestDelegate

- (void)requestDidFinish:(SKRequest *)request
{
  [self validateReceipt:NO];
}

- (void)request:(SKRequest *)request didFailWithError:(NSError *)error
{
  [self appstoreError:error];
}

@end
