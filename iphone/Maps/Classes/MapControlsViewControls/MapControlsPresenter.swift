final class MapControlsPresenter {
  private unowned let view: IMapControlsView
  private let interactor: IMapControlsInteractor
  private let router: IMapControlsRouter

  private var menuMode: MapControlsMenuMode = .inactive
  private var previousMenuMode: MapControlsMenuMode = .inactive
  private var tipType: TipType = .none
  private var afterBookingData: PromoAfterBookingData?

  private var isPoint2PointSelected = false

  private var allButtonsVisible: Bool = true {
    didSet {
      layersButtonVisible = allButtonsVisible
      sideButtonsVisible = allButtonsVisible
    }
  }

  private var sideButtonsVisible: Bool = true {
    didSet {
      view.setSideButtonsVisible(sideButtonsVisible)
    }
  }

  private var layersButtonVisible: Bool = true {
    didSet {
      view.setLayersButtonVisible(layersButtonVisible)
    }
  }

  init(view: IMapControlsView, interactor: IMapControlsInteractor, router: IMapControlsRouter) {
    self.view = view
    self.interactor = interactor
    self.router = router
  }

  private func logTutorialEvent(_ eventName: String, options: [String: Any]) {
    let params = [kStatType: tipType.rawValue]
    Statistics.logEvent(eventName, withParameters: params.reduce(into: options, { $0[$1.0] = $1.1 }))
  }
}

extension MapControlsPresenter: IMapControlsPresenter {
  func viewDidLoad() {
    interactor.setPlacePageSelectedCallback { [weak self] in
      guard let self = self else { return }
      self.view.hidePlacePage()
      NetworkPolicy.shared().callOnlineApi { _ in
        self.view.showPlacePage()
        if self.allButtonsVisible {
          self.layersButtonVisible = false
        }
      }
    } deselectedCallback: { [weak self] switchFullScreen in
      guard let self = self else { return }
      self.view.hidePlacePage()
      if self.allButtonsVisible {
        self.layersButtonVisible = true
      }

      let isSearchResult = MWMSearchManager.manager().state == .result
      let isNavigationDashboardHidden = MWMNavigationDashboardManager.shared().state == .hidden
      if isSearchResult {
        if isNavigationDashboardHidden {
          MWMSearchManager.manager().state = .mapSearch
        } else {
          MWMSearchManager.manager().state = .hidden
        }
      }

      if DeepLinkHandler.shared.isLaunchedByDeeplink || !switchFullScreen {
        return
      }

      let isSearchHidden = MWMSearchManager.manager().state == .hidden
      if isSearchHidden && isNavigationDashboardHidden {
        self.allButtonsVisible = !self.allButtonsVisible
      }
    } updatedCallback: {}

    interactor.setMyPositionModeCallback { [weak self] in self?.view.setLocationButtonMode($0) }
  }
  
  func zoomIn() {
    interactor.zoomIn()
  }

  func zoomOut() {
    interactor.zoomOut()
  }

  func switchMyPosition() {
    interactor.switchMyPositionMode()
  }

  func search() {
    router.search()
  }

  func route() {
    isPoint2PointSelected.toggle()
    MWMRouter.enableAutoAddLastLocation(false)
    if (isPoint2PointSelected) {
      MWMMapViewControlsManager.manager().onRoutePrepare()
    } else {
      MWMRouter.stopRouting()
    }
  }

  func discovery() {
    router.discovery()
  }

  func bookmarks() {
    router.bookmarks()
  }

  func menu() {
    if menuMode == .inactive {
      menuMode = .full
      view.showMenu(.full)
    } else {
      menuMode = .inactive
      view.showMenu(.inactive)
    }
  }

  func layers() {
    menuMode = .layers
    view.showMenu(.layers)
  }

  func hideMenu() {
    menuMode = previousMenuMode
    view.showMenu(menuMode)
  }

  func showAdditionalViews() {
    if MWMRouter.isRoutingActive() || MWMRouter.hasSavedRoute() {
      return
    }

    if MWMSearchManager.manager().state != .hidden {
      return
    }

    if menuMode != .inactive {
      return
    }

    if DeepLinkHandler.shared.isLaunchedByDeeplink {
      return
    }

    afterBookingData = ABTestManager.manager().promoAfterBookingCampaign.afterBookingData
    if afterBookingData!.enabled {
      view.showAfterBoooking(afterBookingData!)
      MWMEye.promoAfterBookingShown(withCityId: afterBookingData!.promoId)
      return
    }

    tipType = MWMEye.getTipType()
    if tipType != .none {
      allButtonsVisible = true
      view.showTutorialType(tipType)
    }
  }

  func onTutorialTarget() {
    logTutorialEvent(kStatTipsTricksClick, options: [:])
    MWMEye.tipClicked(with: tipType, event: .action)
    view.hideTutorial()
  }

  func onTutorialCancel() {
    logTutorialEvent(kStatTipsTricksClose, options: [kStatOption: kStatGotIt])
    MWMEye.tipClicked(with: tipType, event: .gotIt)
    view.hideTutorial()
  }

  func onTutorialScreenTap() {
    logTutorialEvent(kStatTipsTricksClose, options: [kStatOption: kStatOffscreen])
  }

  func onAfterBooking() {
    guard let afterBookingData = afterBookingData, let url = URL(string: afterBookingData.promoUrl) else { return }
    router.openCatalogUrl(url)
  }
}
