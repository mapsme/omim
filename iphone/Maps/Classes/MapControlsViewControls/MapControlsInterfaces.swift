protocol IMapControlsView: AnyObject {
  func showMenu(_ mode: MapControlsMenuMode)
  func showPlacePage()
  func hidePlacePage()
  func updatePlacePage()
  func setSideButtonsVisible(_ visible: Bool)
  func setLayersButtonVisible(_ visible: Bool)
  func showTutorialType(_ tutorialType: TipType)
  func hideTutorial()
  func showAfterBoooking(_ data: PromoAfterBookingData)
  func setLocationButtonMode(_ mode: MyPositionMode)
}

protocol IMapControlsPresenter {
  func viewDidLoad()
  func zoomIn()
  func zoomOut()
  func switchMyPosition()
  func search()
  func route()
  func discovery()
  func bookmarks()
  func menu()
  func layers()
  func hideMenu()
  func showAdditionalViews()
  func onTutorialTarget()
  func onTutorialCancel()
  func onTutorialScreenTap()
  func onAfterBooking()
}

typealias MapObjectSelectedClosure = () -> Void
typealias MapObjectDeselectedClosure = (Bool) -> Void
typealias MapObjectUpdatedClosure = () -> Void
typealias MyPositionModeChangedClosure = (MyPositionMode) -> Void

protocol IMapControlsInteractor {
  func setPlacePageSelectedCallback(_ selected: @escaping MapObjectSelectedClosure,
                                    deselectedCallback: @escaping MapObjectDeselectedClosure,
                                    updatedCallback: @escaping MapObjectUpdatedClosure)
  func setMyPositionModeCallback(_ callback: @escaping MyPositionModeChangedClosure)
  func zoomIn()
  func zoomOut()
  func switchMyPositionMode()
}

protocol IMapControlsRouter {
  func search()
  func discovery()
  func bookmarks()
  func openCatalogUrl(_ url: URL)
}

enum MapControlsMenuMode {
  case full
  case layers
  case inactive
  case hidden
}

