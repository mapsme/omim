final class TrafficButtonViewController: MWMViewController {
  private enum Const {
    static let lightAnimationImages = [UIImage(named: "btn_traffic_update_light_1")!,
                                       UIImage(named: "btn_traffic_update_light_2")!,
                                       UIImage(named: "btn_traffic_update_light_3")!]
    static let darkAnimationImages = [UIImage(named: "btn_traffic_update_dark_1")!,
                                      UIImage(named: "btn_traffic_update_dark_2")!,
                                      UIImage(named: "btn_traffic_update_dark_3")!]
  }

  typealias ShowMenuClosure = () -> Void
  let showMenuHandler: ShowMenuClosure

  init(showMenuHandler: @escaping ShowMenuClosure) {
    self.showMenuHandler = showMenuHandler

    super.init(nibName: nil, bundle: nil)

    MapOverlayManager.add(self)
    StyleManager.shared.addListener(self)
  }

  required init?(coder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }

  deinit {
    MapOverlayManager.remove(self)
    StyleManager.shared.removeListener(self)
  }

  override func viewDidLoad() {
    super.viewDidLoad()

    applyTheme()
  }

  @IBAction func onTrafficButton(_ sender: UIButton) {
    if MapOverlayManager.trafficEnabled() {
      MapOverlayManager.setTrafficEnabled(false)
    } else if MapOverlayManager.transitEnabled() {
      MapOverlayManager.setTransitEnabled(false)
    } else if MapOverlayManager.isoLinesEnabled() {
      MapOverlayManager.setIsoLinesEnabled(false)
    } else if MapOverlayManager.guidesEnabled() {
      MapOverlayManager.setGuidesEnabled(false)
    } else {
      showMenuHandler()
    }
  }

  private func handleTrafficState(_ state: MapOverlayTrafficState) {
    let btn = self.view as! MWMButton

    switch state {
    case .disabled:
      fatalError("Incorrect traffic manager state.")
    case .enabled:
      btn.imageName = "btn_traffic_on";
    case .waitingData:
      let iv = btn.imageView
      iv?.animationImages = UIColor.isNightMode() ? Const.darkAnimationImages : Const.lightAnimationImages
      iv?.animationDuration = 0.8
      iv?.startAnimating()
    case .outdated:
      btn.imageName = "btn_traffic_outdated";
    case .noData:
      btn.imageName = "btn_traffic_on";
      Toast.toast(withText: "traffic_data_unavailable").show()
    case .networkError:
      MapOverlayManager.setTrafficEnabled(false)
      MWMAlertViewController.activeAlert().presentNoConnectionAlert()
    case .expiredData:
      btn.imageName = "btn_traffic_outdated";
      Toast.toast(withText: "traffic_update_maps_text").show()
    case .expiredApp:
      btn.imageName = "btn_traffic_outdated";
      Toast.toast(withText: "traffic_update_app_message").show()
    @unknown default:
      fatalError()
    }
  }

  private func handleIsolinesState(_ state: MapOverlayIsolinesState) {
    switch state {
    case .disabled:
      break
    case .enabled:
      if !MapOverlayManager.isolinesVisible() {
        Toast.toast(withText: "isolines_toast_zooms_1_10").show()
        Statistics.logEvent(kStatMapToastShow, withParameters: [kStatType: kStatIsolines])
      }
    case .expiredData:
      MWMAlertViewController.activeAlert().presentInfoAlert("isolines_activation_error_dialog")
      MapOverlayManager.setIsoLinesEnabled(false)
    case .noData:
      MWMAlertViewController.activeAlert().presentInfoAlert("isolines_location_error_dialog")
      MapOverlayManager.setIsoLinesEnabled(false)
    @unknown default:
      fatalError()
    }
  }

  private func hanldeGuidesState(_ state: MapOverlayGuidesState) {
    switch state {
    case .disabled, .enabled:
      break
    case .hasData:
      performOnce({
        Toast.toast(withText: "routes_layer_is_on_toast").show(withAlignment: .top)
      }, "routes_layer_is_on_toast")
    case .networkError:
      Toast.toast(withText: "connection_error_toast_guides").show()
    case .fatalNetworkError:
      MapOverlayManager.setGuidesEnabled(false)
      MWMAlertViewController.activeAlert().presentNoConnectionAlert()
    case .noData:
      Toast.toast(withText: "no_routes_in_the_area_toast").show()
    @unknown default:
      fatalError()
    }
  }
}

extension TrafficButtonViewController: MapOverlayManagerObserver {
  func onTrafficStateUpdated() {
    applyTheme()
  }

  func onGuidesStateUpdated() {
    applyTheme()
  }

  func onTransitStateUpdated() {
    applyTheme()
  }

  func onIsoLinesStateUpdated() {
    applyTheme()
  }
}

extension TrafficButtonViewController: ThemeListener {
  override func applyTheme() {
    let btn = self.view as! MWMButton
    btn.imageView?.stopAnimating()

    if MapOverlayManager.trafficEnabled() {
      handleTrafficState(MapOverlayManager.trafficState())
    } else if MapOverlayManager.transitEnabled() {
      btn.imageName = "btn_subway_on";
      if MapOverlayManager.transitState() == .noData {
        Toast.toast(withText: "subway_data_unavailable").show()
      }
    } else if MapOverlayManager.isoLinesEnabled() {
      btn.imageName = "btn_isoMap_on";
      handleIsolinesState(MapOverlayManager.isolinesState())
    } else if MapOverlayManager.guidesEnabled() {
      btn.imageName = "btn_layers_off";
      hanldeGuidesState(MapOverlayManager.guidesState())
    } else {
      btn.imageName = "btn_layers";
    }
  }
}
