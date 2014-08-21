#include "BookMarkSplitPanel.hpp"
#include "SceneRegister.hpp"
#include "MapsWithMeForm.hpp"
#include "AppResourceId.h"
#include "Constants.hpp"
#include "Utils.hpp"
#include "MapsWithMeForm.hpp"
#include "BookMarkUtils.hpp"

#include "../../../map/framework.hpp"
#include "../../../map/measurement_utils.hpp"
#include "../../../platform/settings.hpp"
#include "../../../platform/tizen_utils.hpp"
#include "../../../base/logging.hpp"
#include "../../../base/math.hpp"
#include "../../../std/sstream.hpp"
#include <FUixSensor.h>

using namespace Tizen::Base;
using namespace Tizen::Ui;
using namespace Tizen::Ui::Controls;
using namespace Tizen::Ui::Scenes;
using namespace Tizen::Graphics;
using namespace consts;
using namespace bookmark;
using namespace Tizen::Uix::Sensor;
using Tizen::Base::Collection::ArrayList;

BookMarkSplitPanel::BookMarkSplitPanel()
:m_pMainForm(0)
{
}

BookMarkSplitPanel::~BookMarkSplitPanel(void)
{
}

bool BookMarkSplitPanel::Construct(const Tizen::Graphics::FloatRectangle& rect)
{
  SplitPanel::Construct(rect, SPLIT_PANEL_DIVIDER_STYLE_FIXED, SPLIT_PANEL_DIVIDER_DIRECTION_HORIZONTAL);

  Panel * m_pFirstPanel = new Panel();
  FloatRectangle firstRect = rect;
  firstRect.height /=2;
  m_pFirstPanel->Construct(firstRect);

  m_pFirstPanel->SetBackgroundColor(white_bkg);
  m_pButton = new Button;
  m_pButton->Construct(FloatRectangle(btwWdth, btwWdth, rect.width - 2 * btwWdth, lstItmHght - 2 * btwWdth));
  m_pButton->SetColor(BUTTON_STATUS_NORMAL, white_bkg);
  m_pButton->SetColor(BUTTON_STATUS_PRESSED, white_bkg);
  m_pButton->SetText(GetString(IDS_SHARE));
  m_pButton->SetTextColor(black);
  m_pButton->SetActionId(ID_SHARE_BUTTON);
  m_pButton->AddActionEventListener(*this);

  m_pLabel = new Label();
  m_pLabel->Construct(Rectangle(btwWdth, btwWdth, rect.width - btnSz - btwWdth - editBtnSz, markPanelHeight - 2 * btwWdth), GetHeaderText());
  m_pLabel->SetTextHorizontalAlignment(ALIGNMENT_LEFT);
  m_pLabel->SetTextVerticalAlignment(ALIGNMENT_TOP);
  m_pLabel->SetTextColor(txt_main_black);
  m_pLabel->SetTextConfig(mainFontSz ,LABEL_TEXT_STYLE_NORMAL);

  m_pLabel->AddTouchEventListener(*this);

  m_pList = new ListView();
  m_pList->Construct(firstRect, true, SCROLL_STYLE_FIXED);
  m_pList->SetItemProvider(*this);
  m_pList->AddListViewItemEventListener(*this);
  m_pList->SetBackgroundColor(white_bkg);

  m_pFirstPanel->AddControl(m_pButton);
  m_pFirstPanel->AddControl(m_pList);
  m_pFirstPanel->AddControl(m_pLabel);

  m_pSecondPanel = new Panel();
  m_pSecondPanel->Construct(rect);
  m_pSecondPanel->SetBackgroundColor(Color(0,0,0, 100));
  m_pSecondPanel->AddTouchEventListener(*this);

  SetDividerPosition(rect.height / 2);
  SetPane(m_pFirstPanel, SPLIT_PANEL_PANE_ORDER_FIRST);
  SetPane(m_pSecondPanel, SPLIT_PANEL_PANE_ORDER_SECOND);

  m_sensorManager.Construct();
  if (m_sensorManager.IsAvailable(SENSOR_TYPE_MAGNETIC))
    m_sensorManager.AddSensorListener(*this, SENSOR_TYPE_MAGNETIC, 50, true);
  m_northAzimuth = 0;

  return true;
}

void BookMarkSplitPanel::SetMainForm(MapsWithMeForm * pMainForm)
{
  m_pMainForm = pMainForm;
}

void BookMarkSplitPanel::OnTouchPressed (Tizen::Ui::Control const & source,
    Tizen::Graphics::Point const & currentPosition,
    Tizen::Ui::TouchEventInfo const & touchInfo)
{
  if (m_pMainForm)
    m_pMainForm->OnFormBackRequested(*m_pMainForm);
}

void BookMarkSplitPanel::Enable()
{
  UpdateState();
  SetShowState(true);

}

void BookMarkSplitPanel::Disable()
{
  SetShowState(false);
}

void BookMarkSplitPanel::OnActionPerformed(Tizen::Ui::Control const & source, int actionId)
{
  switch(actionId)
  {
    case ID_SHARE_BUTTON:
    {
      String textValSMS = GetBMManager().GetSMSTextMark(GetBMManager().GetCurMark());
      String textValEmail = GetBMManager().GetEmailTextMark(GetBMManager().GetCurMark());
      ArrayList * pList = new ArrayList;
      pList->Construct();
      pList->Add(new String(textValSMS));
      pList->Add(new String(textValEmail));
      pList->Add(new Boolean(false)); //not my position but mark
      SceneManager * pSceneManager = SceneManager::GetInstance();
      pSceneManager->GoForward(ForwardSceneTransition(SCENE_SHARE_POSITION,
          SCENE_TRANSITION_ANIMATION_TYPE_LEFT, SCENE_HISTORY_OPTION_ADD_HISTORY, SCENE_DESTROY_OPTION_KEEP), pList);
      break;
    }
  }
  Invalidate(true);
}

Tizen::Ui::Controls::ListItemBase * BookMarkSplitPanel::CreateHeaderItem (float itemWidth)
{
  CustomItem * pItem = new CustomItem();

  pItem->Construct(FloatDimension(itemWidth, headerItemHeight), LIST_ANNEX_STYLE_NORMAL);
  FloatRectangle imgRect(itemWidth - btwWdth - btnSz, btwWdth, btnSz, btnSz);
  if (IsBookMark())
  {
    FloatRectangle editRect = imgRect;
    editRect.x -= (editBtnSz);
    editRect.y = 0;
    editRect.height = editBtnSz;
    editRect.width = editBtnSz;
    pItem->AddElement(editRect, EDIT_BUTTON, *GetBitmap(IDB_PLACE_PAGE_EDIT_BUTTON), null, null);
    pItem->AddElement(imgRect, STAR_BUTTON, *GetBitmap(IDB_PLACE_PAGE_BUTTON_SELECTED), null, null);
  }
  else
  {
    pItem->AddElement(imgRect, STAR_BUTTON, *GetBitmap(IDB_PLACE_PAGE_BUTTON), null, null);
  }
  return pItem;
}

Tizen::Ui::Controls::ListItemBase * BookMarkSplitPanel::CreateSettingsItem (float itemWidth)
{
  BookMarkManager & mngr = GetBMManager();
  CustomItem * pItem = new CustomItem();

  pItem->Construct(FloatDimension(itemWidth, settingsItemHeight), LIST_ANNEX_STYLE_NORMAL);

  FloatRectangle imgRect(btwWdth, topHght, imgWdth, imgHght);
  if (IsBookMark())
  {
    FloatRectangle colorImgRect(imgRect);
    colorImgRect.x = itemWidth - btwWdth - imgWdth;
    EColor color = mngr.GetCurBookMarkColor();
    pItem->AddElement(colorImgRect, COLOR_IMG, *GetBitmap(GetColorPPBM(color)), null, null);
  }
  FloatRectangle txtRect = imgRect;
  txtRect.width = itemWidth - 3 * imgWdth - 4 * btwWdth;
  txtRect.x = 2 * imgWdth + 2 * btwWdth;
  pItem->AddElement(txtRect, DISTANCE_TXT, GetDistanceText(), mainFontSz, txt_main_black, txt_main_black, txt_main_black);
  String latText, lonText;
  GetLocationLatLonText(latText, lonText);
  txtRect.y += imgHght;
  pItem->AddElement(txtRect, POSITION_LAT_TXT, latText, mainFontSz, txt_main_black, txt_main_black, txt_main_black);
  txtRect.y += imgHght;
  pItem->AddElement(txtRect, POSITION_LON_TXT, lonText, mainFontSz, txt_main_black, txt_main_black, txt_main_black);
  return pItem;
}

Tizen::Ui::Controls::ListItemBase * BookMarkSplitPanel::CreateItem (int index, float itemWidth)
{
  switch(index)
  {
    case HEADER_ITEM: return CreateHeaderItem(itemWidth);
    case SETTINGS_ITEM: return CreateSettingsItem(itemWidth);
  };

  return 0;
}

void BookMarkSplitPanel::UpdateState()
{
  m_pLabel->SetText(GetHeaderText());
  int const listSz = IsBookMark() ? allItemsHeight : headerSettingsHeight;
  m_pList->SetSize(m_pList->GetWidth(), listSz);
  m_pList->UpdateList();
  UpdateCompass();
  m_pButton->SetPosition(btwWdth, listSz + btwWdth);
  SetDividerPosition(listSz + lstItmHght);
  Invalidate(true);
}

bool  BookMarkSplitPanel::DeleteItem (int index, Tizen::Ui::Controls::ListItemBase * pItem, float itemWidth)
{
  delete pItem;
  pItem = null;
  return true;
}
int BookMarkSplitPanel::GetItemCount(void)
{
  return 2;
}
// IListViewItemEventListener
void BookMarkSplitPanel::OnListViewItemStateChanged(Tizen::Ui::Controls::ListView & listView, int index, int elementId, Tizen::Ui::Controls::ListItemStatus status)
{
  if (index == HEADER_ITEM)
  {
    if (elementId == STAR_BUTTON)
    {
      BookMarkManager & mngr = GetBMManager();
      if (IsBookMark())
      {
        mngr.RemoveCurBookMark();
      }
      else
      {
        mngr.AddCurMarkToBookMarks();
      }
    }
    if (elementId == EDIT_BUTTON)
    {
      SceneManager * pSceneManager = SceneManager::GetInstance();
      pSceneManager->GoForward(ForwardSceneTransition(SCENE_PLACE_PAGE_SETTINGS,
          SCENE_TRANSITION_ANIMATION_TYPE_LEFT, SCENE_HISTORY_OPTION_ADD_HISTORY, SCENE_DESTROY_OPTION_KEEP));
    }
  }
  if (index == SETTINGS_ITEM)
  {
    if (elementId == COLOR_IMG)
    {
      SceneManager * pSceneManager = SceneManager::GetInstance();
      pSceneManager->GoForward(ForwardSceneTransition(SCENE_SELECT_COLOR,
          SCENE_TRANSITION_ANIMATION_TYPE_LEFT, SCENE_HISTORY_OPTION_ADD_HISTORY, SCENE_DESTROY_OPTION_KEEP));
    }
    if (elementId == POSITION_LAT_TXT || elementId == POSITION_LON_TXT)
    {
      bool bShowDeg = true;
      Settings::Get(SHOW_DEG, bShowDeg);
      bShowDeg = !bShowDeg;
      Settings::Set(SHOW_DEG, bShowDeg);
    }
  }

  UpdateState();
}

void BookMarkSplitPanel::OnListViewItemLongPressed(Tizen::Ui::Controls::ListView & listView, int index, int elementId, bool & invokeListViewItemCallback)
{
  if (index == SETTINGS_ITEM)
  {
    if (elementId == POSITION_LAT_TXT || elementId == POSITION_LON_TXT)
    {
      Tizen::Base::String latText;
      Tizen::Base::String lonText;
      GetLocationLatLonText(latText, lonText);
      latText.Append(", ");
      latText.Append(lonText);

      Clipboard* pClipboard = Clipboard::GetInstance();
      ClipboardItem item;
      item.Construct(Tizen::Ui::CLIPBOARD_DATA_TYPE_TEXT, latText);
      pClipboard->CopyItem(item);

      String message = GetString(IDS_COPIED_TO_CLIPBOARD);
      message.Replace("%1$s", latText);
      MessageBoxOk("", message);
    }
  }
}

Tizen::Base::String BookMarkSplitPanel::GetHeaderText() const
{
  BookMarkManager & mngr = GetBMManager();
  String res = GetMarkName(GetCurMark());
  res.Append("\n");
  if (IsBookMark())
    res.Append(mngr.GetCurrentCategoryName());
  else
    res.Append(GetMarkType(GetCurMark()));
  return res;
}

Tizen::Base::String BookMarkSplitPanel::GetDistanceText() const
{
  return GetDistance(GetCurMark());
}

void BookMarkSplitPanel::GetLocationLatLonText(Tizen::Base::String & latText, Tizen::Base::String & lonText) const
{
  if (!GetCurMark())
    return;
  double lat, lon;
  GetCurMark()->GetLatLon(lat, lon);
  bool bShowDeg = true;
  Settings::Get(SHOW_DEG, bShowDeg);
  string lat_str, lon_str;
  if (bShowDeg)
    MeasurementUtils::FormatLatLon(lat, lon, lat_str, lon_str);
  else
    MeasurementUtils::FormatLatLonAsDMS(lat, lon, lat_str, lon_str);
  lonText = lon_str.c_str();
  latText = lat_str.c_str();
}

bool BookMarkSplitPanel::IsBookMark() const
{
  return bookmark::IsBookMark(GetCurMark());
}

UserMark const * BookMarkSplitPanel::GetCurMark() const
{
  return GetBMManager().GetCurMark();
}

namespace
{
void AngleIn2Pi(double & angle)
{
  double const pi2 = 2.0 * math::pi;
  if (angle <= 0.0)
    angle += pi2;
  else if (angle > pi2)
    angle -= pi2;
}
}

void BookMarkSplitPanel::UpdateCompass()
{
  Tizen::Graphics::Canvas * pCanvas =  m_pList->GetCanvasN();
  if (pCanvas)
  {
    pCanvas->FillRectangle(consts::white_bkg, Rectangle(2, headerItemHeight + 2, 500, 500));
    Bitmap const * pBTM = GetBitmap(IDB_PLACE_PAGE_COMPASS);
    int const imgHeighDiv2 = pBTM->GetHeight() / 2;
    int const centerX = btwWdth + imgHeighDiv2;
    int const centerY = headerItemHeight + btwWdth + imgHeighDiv2;
    double const dAzimut = GetAzimuth(GetCurMark(), m_northAzimuth);
    double dRotateAngle = dAzimut - (math::pi / 2);
    AngleIn2Pi(dRotateAngle);
    pCanvas->DrawBitmap(Point(centerX, centerY), *pBTM, Point(imgHeighDiv2, imgHeighDiv2), my::RadToDeg(dRotateAngle));
    delete pCanvas;
  }
}
void BookMarkSplitPanel::OnDataReceived(SensorType sensorType, SensorData & sensorData, result r)
{
  MagneticSensorData & data = static_cast< MagneticSensorData& >(sensorData);
  m_northAzimuth = atan2(data.y, data.x) - (math::pi / 2);
  AngleIn2Pi(m_northAzimuth);

  UpdateCompass();
}
