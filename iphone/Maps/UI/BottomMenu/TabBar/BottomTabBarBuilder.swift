@objc class BottomTabBarBuilder: NSObject {
  @objc static func build(mapViewController: MapViewController, controlsManager: MWMMapViewControlsManager) -> BottomTabBarViewController {
    let viewController = BottomTabBarViewController(nibName: nil, bundle: nil)
    return viewController
  }
}
