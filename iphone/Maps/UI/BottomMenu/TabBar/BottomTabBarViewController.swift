protocol BottomTabBarViewProtocol: class {
//  var presenter: BottomTabBarPresenterProtocol! { get set }
  var isHidden: Bool { get }
  var isLayersBadgeHidden: Bool { get set }
  var isApplicationBadgeHidden: Bool { get set }
}

protocol BottomTabBarViewControllerDelegate: AnyObject {
  func search()
  func route()
  func discovery()
  func bookmarks()
  func menu()
}

final class BottomTabBarViewController: UIViewController {
//  var presenter: BottomTabBarPresenterProtocol!
  var delegate: BottomTabBarViewControllerDelegate?
  
  @IBOutlet var searchButton: MWMButton!
  @IBOutlet var routeButton: MWMButton!
  @IBOutlet var discoveryButton: MWMButton!
  @IBOutlet var bookmarksButton: MWMButton!
  @IBOutlet var moreButton: MWMButton!
  @IBOutlet var downloadBadge: UIView!
  
  var isLayersBadgeHidden: Bool = true {
    didSet {
      updateBadge()
    }
  }

  var isApplicationBadgeHidden: Bool = true {
    didSet {
      updateBadge()
    }
  }

//  var tabBarView: BottomTabBarView {
//    return view as! BottomTabBarView
//  }

  override func viewDidLoad() {
    super.viewDidLoad()
//    presenter.configure()
    updateBadge()

    MWMSearchManager.add(self)
//    MWMNavigationDashboardManager.add(self)
  }
  
  override func viewDidAppear(_ animated: Bool) {
    super.viewDidAppear(animated)
  }
  
  deinit {
    MWMSearchManager.remove(self)
//    MWMNavigationDashboardManager.remove(self)
  }
  
  @IBAction func onSearchButtonPressed(_ sender: Any) {
    delegate?.search()
//    presenter.onSearchButtonPressed()
  }
  
  @IBAction func onPoint2PointButtonPressed(_ sender: Any) {
    delegate?.route()
//    presenter.onPoint2PointButtonPressed()
  }
  
  @IBAction func onDiscoveryButtonPressed(_ sender: Any) {
    delegate?.discovery()
//    presenter.onDiscoveryButtonPressed()
  }
  
  @IBAction func onBookmarksButtonPressed(_ sender: Any) {
    delegate?.bookmarks()
//    presenter.onBookmarksButtonPressed()
  }
  
  @IBAction func onMenuButtonPressed(_ sender: Any) {
    delegate?.menu()
//    presenter.onMenuButtonPressed()
  }
  
  private func updateBadge() {
    downloadBadge.isHidden = isApplicationBadgeHidden && isLayersBadgeHidden
  }
}

//extension BottomTabBarViewController: BottomTabBarViewProtocol {
//
//}
//
// MARK: - MWMNavigationDashboardObserver

//extension BottomTabBarViewController: MWMNavigationDashboardObserver {
//  func onNavigationDashboardStateChanged() {
//    let state = MWMNavigationDashboardManager.shared().state
//    self.isHidden = state != .hidden;
//  }
//}

// MARK: - MWMSearchManagerObserver

extension BottomTabBarViewController: MWMSearchManagerObserver {
  func onSearchManagerStateChanged() {
    let state = MWMSearchManager.manager().state;
    self.searchButton.isSelected = state != .hidden
  }
}
