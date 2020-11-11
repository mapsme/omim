final class MapControlsRouter {
  private let mapViewController: MapViewController
  private weak var coordinator: BookmarksCoordinator?
  private weak var searchManager = MWMSearchManager.manager()

  init(_ mapViewController: MapViewController, bookmarksCoordinator: BookmarksCoordinator?) {
    self.mapViewController = mapViewController
    self.coordinator = bookmarksCoordinator
  }
}

extension MapControlsRouter: IMapControlsRouter {
  func search() {
    if searchManager?.state == .hidden {
      searchManager?.state = .default
    } else {
      searchManager?.state = .hidden
    }
  }

  func discovery() {
    NetworkPolicy.shared().callOnlineApi { [weak self] canUseNetwork in
      let vc = MWMDiscoveryController.instance(withConnection: canUseNetwork)
      self?.mapViewController.navigationController?.pushViewController(vc!, animated: true)
    }
  }

  func bookmarks() {
    coordinator?.open()
  }

  func openCatalogUrl(_ url: URL) {
    mapViewController.openCatalogAbsoluteUrl(url, animated: true, utm: .bookingPromo)
  }
}
