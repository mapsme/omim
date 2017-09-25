final class NavigationInfoArea: AvailableArea {
  override func isAreaAffectingView(_ other: UIView) -> Bool {
    return !other.navigationInfoAreaAffectDirections.isEmpty
  }

  override func addAffectingView(_ other: UIView) {
    let ov = other.navigationInfoAreaAffectView
    let directions = ov.navigationInfoAreaAffectDirections
    addConstraints(otherView: ov, directions: directions)
  }

  override func notifyObserver() {
    MWMNavigationDashboardManager.updateNavigationInfoAvailableArea(frame)
  }
}

extension UIView {
  @objc var navigationInfoAreaAffectDirections: MWMAvailableAreaAffectDirections { return [] }

  @objc var navigationInfoAreaAffectView: UIView { return self }
}
