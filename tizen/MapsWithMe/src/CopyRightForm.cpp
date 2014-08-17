#include "CopyRightForm.hpp"
#include "SceneRegister.hpp"
#include "MapsWithMeForm.hpp"
#include "AppResourceId.h"
#include "Utils.hpp"
#include "../../../base/logging.hpp"
#include "../../../platform/settings.hpp"
#include "../../../platform/tizen_utils.hpp"
#include <FWeb.h>
#include <FAppApp.h>
#include <FApp.h>

using namespace Tizen::Base;
using namespace Tizen::Ui;
using namespace Tizen::Ui::Controls;
using namespace Tizen::Ui::Scenes;
using namespace Tizen::Web::Controls;

CopyRightForm::CopyRightForm()
{
}

CopyRightForm::~CopyRightForm(void)
{
}

bool CopyRightForm::Initialize(void)
{
  Construct(IDF_USED_LICENSES_FORM);
  return true;
}

result CopyRightForm::OnInitializing(void)
{

  //  web page
  Web * pWeb = static_cast<Web *>(GetControl(IDC_WEB, true));
  Tizen::Base::String url = "file://";
  url += (Tizen::App::App::GetInstance()->GetAppDataPath());
  url += "copyright.html";
  pWeb->LoadUrl(url);

  Button * pButtonBack = static_cast<Button *>(GetControl(IDC_CLOSE_BUTTON, true));
  pButtonBack->SetActionId(ID_CLOSE);
  pButtonBack->AddActionEventListener(*this);

  SetFormBackEventListener(this);
  return E_SUCCESS;
}

void CopyRightForm::OnActionPerformed(Tizen::Ui::Control const & source, int actionId)
{
  switch(actionId)
  {
    case ID_CLOSE:
    {
      SceneManager* pSceneManager = SceneManager::GetInstance();
      pSceneManager->GoBackward(BackwardSceneTransition(SCENE_TRANSITION_ANIMATION_TYPE_RIGHT));
      break;
    }
  }
  Invalidate(true);
}

void CopyRightForm::OnFormBackRequested(Tizen::Ui::Controls::Form & source)
{
  SceneManager * pSceneManager = SceneManager::GetInstance();
  pSceneManager->GoBackward(BackwardSceneTransition(SCENE_TRANSITION_ANIMATION_TYPE_RIGHT));
}
