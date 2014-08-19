#include "SearchForm.hpp"
#include "SceneRegister.hpp"
#include "MapsWithMeForm.hpp"
#include "AppResourceId.h"

#include "../../../map/user_mark.hpp"
#include "../../../map/framework.hpp"
#include "../../../search/result.hpp"
#include "../../../platform/settings.hpp"
#include "../../../platform/tizen_utils.hpp"
#include "../../../base/logging.hpp"

#include <FWeb.h>
#include <FAppApp.h>
#include <FApp.h>
#include "Utils.hpp"
#include "Framework.hpp"
#include "Constants.hpp"

using namespace Tizen::Base;
using namespace Tizen::Ui;
using namespace Tizen::Ui::Controls;
using namespace Tizen::Ui::Scenes;
using namespace Tizen::Base::Collection;
using namespace Tizen::Graphics;
using namespace search;
using namespace consts;

namespace detail
{
typedef vector<String> CategoriesT;
CategoriesT const & GetCategories()
{
  static CategoriesT vr;
  if (vr.empty())
  {
    vr.push_back(GetString(IDS_FOOD));
    vr.push_back(GetString(IDS_HOTEL));
    vr.push_back(GetString(IDS_TOURISM));
    vr.push_back(GetString(IDS_TRANSPORT));
    vr.push_back(GetString(IDS_FUEL));
    vr.push_back(GetString(IDS_SHOP));
    vr.push_back(GetString(IDS_ENTERTAINMENT));
    vr.push_back(GetString(IDS_ATM));
    vr.push_back(GetString(IDS_BANK));
    vr.push_back(GetString(IDS_PARKING));
    vr.push_back(GetString(IDS_TOILET));
    vr.push_back(GetString(IDS_PHARMACY));
    vr.push_back(GetString(IDS_HOSPITAL));
    vr.push_back(GetString(IDS_POST));
    vr.push_back(GetString(IDS_POLICE));
  }
  return vr;
}

Tizen::Graphics::Bitmap const * GetCategoryBitMap(int i)
{
  switch (i)
  {
    case 0: return GetBitmap(IDB_SEARCH_FOOD);
    case 1: return GetBitmap(IDB_SEARCH_HOTEL);
    case 2: return GetBitmap(IDB_SEARCH_TOURISM);
    case 3: return GetBitmap(IDB_SEARCH_TRANSPORT);
    case 4: return GetBitmap(IDB_SEARCH_FUEL);
    case 5: return GetBitmap(IDB_SEARCH_SHOP);
    case 6: return GetBitmap(IDB_SEARCH_ENTERTAINMENT);
    case 7: return GetBitmap(IDB_SEARCH_ATM);
    case 8: return GetBitmap(IDB_SEARCH_BANK);
    case 9: return GetBitmap(IDB_SEARCH_PARKING);
    case 10: return GetBitmap(IDB_SEARCH_TOILET);
    case 11: return GetBitmap(IDB_SEARCH_PHARMACY);
    case 12: return GetBitmap(IDB_SEARCH_HOSPITAL);
    case 13: return GetBitmap(IDB_SEARCH_POST);
    case 14: return GetBitmap(IDB_SEARCH_POLICE);
  }
  return 0;
}

CustomItem * CreateFeatureItem(Result const & val, double itemWidth)
{
  String itemText = val.GetString();
  CustomItem * pItem = new CustomItem();

  pItem->Construct(FloatDimension(itemWidth, lstItmHght), LIST_ANNEX_STYLE_NORMAL);
  pItem->SetBackgroundColor(LIST_ITEM_DRAWING_STATUS_NORMAL, mainMenuGray);
  FloatRectangle imgRect(btwWdth, topHght, imgWdth, imgHght);
  int txtWdht = itemWidth - btwWdth - imgWdth - backWdth;
  pItem->AddElement(FloatRectangle(btwWdth, 15, txtWdht, imgHght), 1, val.GetString(), mainFontSz, white, white, white);
  pItem->AddElement(FloatRectangle(btwWdth, 60.0f, txtWdht, imgHght), 2, val.GetRegionString(), minorFontSz, gray, gray, gray);
  String feature = val.GetFeatureType();
  pItem->AddElement(FloatRectangle(itemWidth - backWdth, 15, backWdth, imgHght), 3, GetFeature(feature), mediumFontSz, gray, gray, gray);
  double lat, lon;
  GetFramework()->GetCurrentPosition(lat, lon);
  double north = 0;
  string distance;
  double azimut;
  GetFramework()->GetDistanceAndAzimut(val.GetFeatureCenter(), lat, lon, north, distance, azimut);
  String dist(distance.c_str());
  pItem->AddElement(FloatRectangle(itemWidth - backWdth, 60.0f, backWdth, imgHght), 4, dist, mainFontSz, gray, gray, gray);

  return pItem;
}

CustomItem * CreateSuggestionItem(String const & val, double itemWidth)
{
  String itemText = val;
  CustomItem * pItem = new CustomItem();

  pItem->Construct(FloatDimension(itemWidth, lstItmHght), LIST_ANNEX_STYLE_NORMAL);
  pItem->SetBackgroundColor(LIST_ITEM_DRAWING_STATUS_NORMAL, green);
  FloatRectangle imgRect(btwWdth, topHght, imgWdth, imgHght);
  pItem->AddElement(imgRect, 0, *GetBitmap(IDB_SUGGESTION_RESULT), null, null);
  pItem->AddElement(FloatRectangle(btwWdth + imgWdth + btwWdth, topHght, itemWidth, imgHght), 1, itemText, mainFontSz, white, white, white);

  return pItem;
}

CustomItem * CreateSearchOnMapItem(double itemWidth)
{
  String itemText = GetString(IDS_SEARCH_ON_MAP);
  CustomItem * pItem = new CustomItem();
  pItem->Construct(FloatDimension(itemWidth, lstItmHght), LIST_ANNEX_STYLE_NORMAL);
  pItem->SetBackgroundColor(LIST_ITEM_DRAWING_STATUS_NORMAL, green);
  pItem->AddElement(FloatRectangle(btwWdth, topHght, itemWidth, imgHght), 1, itemText, mainFontSz, white, white, white);
  return pItem;
}

CustomItem * CreateCategoryItem(int i, double itemWidth)
{
  CustomItem * pItem = new CustomItem();
  pItem->Construct(FloatDimension(itemWidth, lstItmHght), LIST_ANNEX_STYLE_NORMAL);
  pItem->SetBackgroundColor(LIST_ITEM_DRAWING_STATUS_NORMAL, black);
  FloatRectangle imgRect(btwWdth, topHght, imgWdth, imgHght);
  pItem->AddElement(imgRect, 0, *GetCategoryBitMap(i), null, null);
  pItem->AddElement(FloatRectangle(btwWdth + imgWdth + btwWdth, topHght, itemWidth, imgHght), 1, GetCategories()[i], mainFontSz, white, white, white);

  return pItem;
}

bool IsHaveSuggestions(search::Results const &  curResults)
{
  for (size_t i = 0; i < curResults.GetCount(); ++i)
  {
    Result res = curResults.GetResult(i);
    if (res.GetResultType() == Result::RESULT_SUGGEST_FROM_FEATURE || res.GetResultType() == Result::RESULT_SUGGEST_PURE)
      return true;
  }
  return false;
}

} // detail

using namespace detail;


SearchForm::SearchForm()
:m_searchBar(0)
{
}

SearchForm::~SearchForm(void)
{
}

bool SearchForm::Initialize(void)
{
  Construct(IDF_SEARCH_FORM);
  return true;
}

result SearchForm::OnInitializing(void)
{
  m_searchBar = static_cast<SearchBar *>(GetControl(IDC_SEARCHBAR, true));
  m_searchBar->SetMode(SEARCH_BAR_MODE_INPUT);
  m_searchBar->AddActionEventListener(*this);
  m_searchBar->AddTextEventListener(*this);
  m_searchBar->AddKeypadEventListener(*this);

  ListView * pList = static_cast<ListView *>(GetControl(IDC_LISTVIEW, true));
  pList->SetItemProvider(*this);
  pList->AddListViewItemEventListener(*this);
  pList->AddScrollEventListener(*this);

  SetFormBackEventListener(this);

  return E_SUCCESS;
}

void SearchForm::OnActionPerformed(Tizen::Ui::Control const & source, int actionId)
{
  if (actionId == m_searchBar->GetButtonActionId())
  {
    GetFramework()->CancelInteractiveSearch();
    SceneManager * pSceneManager = SceneManager::GetInstance();
    pSceneManager->GoBackward(BackwardSceneTransition(SCENE_TRANSITION_ANIMATION_TYPE_RIGHT));
  }
}

void SearchForm::OnTextValueChanged(Tizen::Ui::Control const & source)
{
  Search(GetSearchString());
}

void SearchForm::OnSearchResultsReceived(search::Results const & results)
{
  if (results.IsEndMarker())
  {
    if (!results.IsEndedNormal())
      m_curResults.Clear();
    UpdateList();
  }
  else
    m_curResults = results;
}

void SearchForm::OnFormBackRequested(Tizen::Ui::Controls::Form & source)
{
  GetFramework()->CancelInteractiveSearch();
  SceneManager * pSceneManager = SceneManager::GetInstance();
  pSceneManager->GoBackward(BackwardSceneTransition(SCENE_TRANSITION_ANIMATION_TYPE_RIGHT));
}

ListItemBase * SearchForm::CreateItem (int index, float itemWidth)
{
  if(IsShowCategories())
  {
    return CreateCategoryItem(index, itemWidth);
  }
  else
  {
    if (m_curResults.GetCount() == 0)
    {
      String itemText = GetString(IDS_NO_SEARCH_RESULTS_FOUND);
      CustomItem * pItem = new CustomItem();
      pItem->Construct(FloatDimension(itemWidth, lstItmHght), LIST_ANNEX_STYLE_NORMAL);
      pItem->SetBackgroundColor(LIST_ITEM_DRAWING_STATUS_NORMAL, mainMenuGray);
      pItem->AddElement(FloatRectangle(btwWdth, topHght, itemWidth, imgHght), 0, itemText, mainFontSz, white, white, white);
      return pItem;
    }
    else
    {
      if (IsHaveSuggestions(m_curResults))
      {
        Result const & res = m_curResults.GetResult(index);
        if (res.GetResultType() == Result::RESULT_SUGGEST_FROM_FEATURE || res.GetResultType() == Result::RESULT_SUGGEST_PURE)
          return CreateSuggestionItem(res.GetString(), itemWidth);
        else
          return CreateFeatureItem(res, itemWidth);
      }
      else
      {
        if (index == 0)
          return CreateSearchOnMapItem(itemWidth);
        else
        {
          Result const & res = m_curResults.GetResult(index - 1);
          return CreateFeatureItem(res, itemWidth);
        }
      }
    }
  }
}

void SearchForm::OnListViewItemStateChanged(Tizen::Ui::Controls::ListView & listView, int index, int elementId, Tizen::Ui::Controls::ListItemStatus status)
{
  if(IsShowCategories())
  {
    m_searchBar->SetText(GetCategories()[index]);
    Search(GetSearchString());
    Invalidate(true);
  }
  else
  {
    if (m_curResults.GetCount() > 0)
    {
      if (IsHaveSuggestions(m_curResults))
      {
        Result const & res = m_curResults.GetResult(index);
        if (res.GetResultType() == Result::RESULT_SUGGEST_FROM_FEATURE || res.GetResultType() == Result::RESULT_SUGGEST_PURE)
        {
          String s = res.GetString();
          s.Append(L" ");
          m_searchBar->SetText(s);
          Search(GetSearchString());
          Invalidate(true);
        }
        else
        {
          GetFramework()->ShowSearchResult(res);
          SceneManager * pSceneManager = SceneManager::GetInstance();
          pSceneManager->GoBackward(BackwardSceneTransition(SCENE_TRANSITION_ANIMATION_TYPE_RIGHT));
        }
      }
      else
      {
        if (index == 0)
        {
          SearchOnMap();
        }
        else
        {
          Result const & res = m_curResults.GetResult(index - 1);
          GetFramework()->ShowSearchResult(res);
          SceneManager * pSceneManager = SceneManager::GetInstance();
          pSceneManager->GoBackward(BackwardSceneTransition(SCENE_TRANSITION_ANIMATION_TYPE_RIGHT));
        }
      }
    }
  }
}

void SearchForm::OnScrollPositionChanged(Tizen::Ui::Control & source, int scrollPosition)
{
  m_searchBar->HideKeypad();
}

bool SearchForm::DeleteItem (int index, Tizen::Ui::Controls::ListItemBase * pItem, float itemWidth)
{
  delete pItem;
  return true;
}

int SearchForm::GetItemCount(void)
{
  if (IsShowCategories())
  {
    return GetCategories().size();
  }
  else
  {
    if (m_curResults.GetCount() == 0)
      return 1;
    else
    {
      if (IsHaveSuggestions(m_curResults))
        return m_curResults.GetCount();
      else
        return m_curResults.GetCount() + 1;
    }
  }
}

void SearchForm::UpdateList()
{
  ListView * pList = static_cast<ListView *>(GetControl(IDC_LISTVIEW));
  pList->UpdateList();
  pList->RemoveScrollEventListener(*this);
  pList->ScrollToItem(0);
  pList->AddScrollEventListener(*this);
}

void SearchForm::Search(String const & val)
{
  search::SearchParams params;
  params.m_callback = bind(&SearchForm::OnSearchResultsReceived, this, _1);
  params.m_query = FromTizenString(val);

  Tizen::Locales::LanguageCode language;
  if (m_searchBar->GetCurrentLanguage(language) == E_SUCCESS)
    params.SetInputLanguage(CodeFromISO369_2to_1(GetLanguageCode(language)));
  double lat, lon;
  GetFramework()->GetCurrentPosition(lat, lon);
  params.SetPosition(lat, lon);

  GetFramework()->Search(params);
}

String SearchForm::GetSearchString() const
{
  return m_searchBar->GetText();
}

bool SearchForm::IsShowCategories() const
{
  return GetSearchString().IsEmpty();
}

void SearchForm::SearchOnMap()
{
  search::SearchParams params;
  params.m_query = FromTizenString(GetSearchString());
  Tizen::Locales::LanguageCode language;
  if (m_searchBar->GetCurrentLanguage(language) == E_SUCCESS)
    params.SetInputLanguage(CodeFromISO369_2to_1(GetLanguageCode(language)));
  double lat, lon;
  GetFramework()->GetCurrentPosition(lat, lon);
  params.SetPosition(lat, lon);

  GetFramework()->StartInteractiveSearch(params);
  ArrayList * pList = new ArrayList;
  pList->Construct();
  pList->Add(new String(GetSearchString()));
  SceneManager * pSceneManager = SceneManager::GetInstance();
  pSceneManager->GoBackward(BackwardSceneTransition(SCENE_TRANSITION_ANIMATION_TYPE_RIGHT), pList);
}

void SearchForm::OnKeypadActionPerformed (Tizen::Ui::Control & source, Tizen::Ui::KeypadAction keypadAction)
{
  if (keypadAction == KEYPAD_ACTION_SEARCH)
  {
    SearchOnMap();
  }
}

void SearchForm::OnSceneActivatedN(const Tizen::Ui::Scenes::SceneId& previousSceneId,
    const Tizen::Ui::Scenes::SceneId& currentSceneId, Tizen::Base::Collection::IList* pArgs)
{
  if (pArgs != null)
  {
    // Come from Main page in resume mode
    if (pArgs->GetCount() == 1)
    {
      String * pSearchText = dynamic_cast<String *>(pArgs->GetAt(0));
      m_searchBar->SetText(*pSearchText);
      Search(GetSearchString());
    }
    pArgs->RemoveAll(true);
    delete pArgs;
  }
}
