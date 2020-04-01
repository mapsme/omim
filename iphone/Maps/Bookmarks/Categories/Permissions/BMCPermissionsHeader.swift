protocol BMCPermissionsHeaderDelegate {
  func collapseAction(isCollapsed: Bool)
}

final class BMCPermissionsHeader: UIView {
  static private let kUDPermissionWidgetColapsed = "UDPermissionWidgetColapsed"

  @IBOutlet private weak var label: UILabel! {
    didSet {
      label.font = .medium14()
      label.textColor = .blackSecondaryText()
      label.text = L("settings_backup_bookmarks").uppercased()
    }
  }

  @IBOutlet private weak var button: UIButton! {
    didSet {
      button.setImage(#imageLiteral(resourceName: "ic24PxChevronUp"), for: .normal)
      button.tintColor = .blackSecondaryText()
      updateButton()
    }
  }

  var isCollapsed = UserDefaults.standard.bool(forKey: BMCPermissionsHeader.kUDPermissionWidgetColapsed) {
    didSet {
      UserDefaults.standard.set(isCollapsed, forKey: BMCPermissionsHeader.kUDPermissionWidgetColapsed)
      updateButton()
    }
  }

  var delegate: BMCPermissionsHeaderDelegate!

  private func updateButton() {
    UIView.animate(withDuration: kDefaultAnimationDuration) {
      self.button?.imageView?.transform = self.isCollapsed ? .init(rotationAngle: .pi - 0.001) : .identity //fix for rotation direction on expand-collapse
    }
  }

  @IBAction private func buttonAction() {
    delegate.collapseAction(isCollapsed: isCollapsed)
  }
}
