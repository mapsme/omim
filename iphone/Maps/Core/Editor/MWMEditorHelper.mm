#import "MWMEditorHelper.h"
#import "AppInfo.h"
#import "MWMAuthorizationCommon.h"

#include "editor/osm_editor.hpp"

#include "platform/platform.hpp"

@implementation MWMEditorHelper

+ (void)uploadEdits:(void (^)(UIBackgroundFetchResult))completionHandler
{
  if (!osm_auth_ios::AuthorizationHaveCredentials() ||
      Platform::EConnectionType::CONNECTION_NONE == Platform::ConnectionStatus())
  {
    completionHandler(UIBackgroundFetchResultFailed);
  }
  else
  {
    auto const lambda = [completionHandler](osm::Editor::UploadResult result) {
      switch (result)
      {
      case osm::Editor::UploadResult::Success:
        completionHandler(UIBackgroundFetchResultNewData);
        break;
      case osm::Editor::UploadResult::Error:
        completionHandler(UIBackgroundFetchResultFailed);
        break;
      case osm::Editor::UploadResult::NothingToUpload:
        completionHandler(UIBackgroundFetchResultNoData);
        break;
      }
    };
    osm::TKeySecret const keySecret = osm_auth_ios::AuthorizationGetCredentials();
    osm::Editor::Instance().UploadChanges(
        keySecret.first, keySecret.second,
        {{"created_by",
          string("MAPS.ME " OMIM_OS_NAME " ") + AppInfo.sharedInfo.bundleVersion.UTF8String},
         {"bundle_id", NSBundle.mainBundle.bundleIdentifier.UTF8String}},
        lambda);
  }
}

@end
