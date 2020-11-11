protocol BottomMenuInteractorProtocol: class {
  func close()
  func addPlace()
  func downloadRoutes()
  func downloadMaps()
  func openSettings()
  func shareLocation(cell: BottomMenuItemCell)
}

@objc protocol BottomMenuDelegate {
  func addPlace()
  func didFinishAddingPlace()
  func didFinish()
}

class BottomMenuInteractor {
  weak var presenter: BottomMenuPresenterProtocol?
  private weak var viewController: UIViewController?
  private weak var mapViewController: MapViewController?
  private weak var delegate: BottomMenuDelegate?

  init(viewController: UIViewController,
       mapViewController: MapViewController,
       delegate: BottomMenuDelegate) {
    self.viewController = viewController
    self.mapViewController = mapViewController
    self.delegate = delegate
  }
}

extension BottomMenuInteractor: BottomMenuInteractorProtocol {
  func close() {
    delegate?.didFinish()
  }

  func addPlace() {
    Statistics.logEvent(kStatToolbarClick, withParameters: [kStatItem : kStatAddPlace])
    delegate?.addPlace()
  }

  func downloadRoutes() {
    Statistics.logEvent(kStatToolbarClick, withParameters: [kStatItem : kStatDownloadGuides])
    close()
    mapViewController?.openCatalog(animated: true, utm: .toolbarButton)
  }

  func downloadMaps() {
    Statistics.logEvent(kStatToolbarClick, withParameters: [kStatItem : kStatDownloadMaps])
    close()
    mapViewController?.openMapsDownloader(.downloaded)
  }

  func openSettings() {
    Statistics.logEvent(kStatToolbarClick, withParameters: [kStatItem : kStatSettings])
    close()
    mapViewController?.performSegue(withIdentifier: "Map2Settings", sender: nil)
  }

  func shareLocation(cell: BottomMenuItemCell) {
    Statistics.logEvent(kStatToolbarClick, withParameters: [kStatItem : kStatShareMyLocation])
    let lastLocation = LocationManager.lastLocation()
    guard let coordinates = lastLocation?.coordinate else {
      UIAlertView(title: L("unknown_current_position"),
                  message: nil,
                  delegate: nil,
                  cancelButtonTitle: L("ok")).show()
      return;
    }
    guard let viewController = viewController else { return }
    let vc = ActivityViewController.share(forMyPosition: coordinates)
    vc?.present(inParentViewController: viewController, anchorView: cell.anchorView)
  }
}
