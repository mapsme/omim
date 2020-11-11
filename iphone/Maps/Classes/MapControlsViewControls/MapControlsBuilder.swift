import Foundation

@objc final class MapControlsBuilder: NSObject {
  @objc static func build(bookmarksCoordinator: BookmarksCoordinator?) -> MapControlsViewController {
    let mapViewController = MapViewController.shared()!
    let viewController = mapViewController.storyboard!.instantiateViewController(ofType: MapControlsViewController.self)
    bookmarksCoordinator?.mapControlsViewController = viewController
    let interactor = MapControlsInteractor()
    let router = MapControlsRouter(mapViewController, bookmarksCoordinator: bookmarksCoordinator)
    let presenter = MapControlsPresenter(view: viewController, interactor: interactor, router: router)
    viewController.presenter = presenter
    return viewController
  }
}
