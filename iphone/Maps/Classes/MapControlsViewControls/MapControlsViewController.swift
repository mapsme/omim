protocol MapOverlayViewProtocol {
  var bottomView: UIView { get }
}

final class MapControlsViewController: UIViewController {
  var presenter: IMapControlsPresenter!

  @IBOutlet var mapButtonsView: TouchTransparentView!
  @IBOutlet var placePageContainerView: TouchTransparentView!
  @IBOutlet var guidesGalleryContainerView: TouchTransparentView!
  @IBOutlet var guidesVisibleConstraint: NSLayoutConstraint!
  @IBOutlet var guidesHiddenConstraint: NSLayoutConstraint!
  @IBOutlet var mapButtonsBottomConstraint: NSLayoutConstraint!
  @IBOutlet var tabBarContainerView: UIView!
  @IBOutlet var layersButtonContainerView: UIView!
  @IBOutlet var layersButtonVisibleConstraint: NSLayoutConstraint!
  @IBOutlet var layersButtonHiddenConstraint: NSLayoutConstraint!
  @IBOutlet var sideButtonsContainerView: UIView!
  @IBOutlet var sideButtonsVisibleConstraint: NSLayoutConstraint!
  @IBOutlet var sideButtonsHiddenConstraint: NSLayoutConstraint!


  private var placePageViewController: PlacePageViewController?
  private var guidesGalleryViewController: GuidesGalleryViewController?
  private var guidesNavigationBar: GuidesNavigationBarViewController?
  private var menuViewController: UIViewController?
  private var tutorialViewController: TutorialViewController?

  private lazy var tabBarViewController: BottomTabBarViewController = {
    let contoller = BottomTabBarViewController()
    contoller.delegate = self
    return contoller
  }()

  private lazy var trafficButtonViewController: TrafficButtonViewController = {
    TrafficButtonViewController { [weak self] in
      self?.presenter.layers()
    }
  }()

  override func viewDidLoad() {
    super.viewDidLoad()

    tabBarContainerView.addSubview(tabBarViewController.view)
    addChild(tabBarViewController)
    tabBarViewController.didMove(toParent: self)

    layersButtonContainerView.addSubview(trafficButtonViewController.view)
    trafficButtonViewController.view.alignToSuperview()
    addChild(trafficButtonViewController)
    trafficButtonViewController.didMove(toParent: self)

    presenter.viewDidLoad()
  }

  @objc func showAdditionalViews() {
    presenter.showAdditionalViews()
  }

  private func showRegularPlacePage() {
    placePageViewController = PlacePageBuilder.build()
    placePageContainerView.isHidden = false
    placePageContainerView.addSubview(placePageViewController!.view)
    placePageViewController!.view.alignToSuperview()
    addChild(placePageViewController!)
    placePageViewController?.didMove(toParent: self)
    bindBottomView(placePageViewController!)
  }

  private func showGuidesGallery() {
    guidesGalleryViewController = GuidesGalleryBuilder.build()
    guidesGalleryContainerView.isHidden = false
    guidesGalleryContainerView.addSubview(guidesGalleryViewController!.view)
    guidesGalleryViewController!.view.alignToSuperview()
    addChild(guidesGalleryViewController!)
    guidesGalleryViewController!.didMove(toParent: self)

    guidesVisibleConstraint.priority = .defaultLow
    guidesHiddenConstraint.priority = .defaultHigh
    guidesGalleryContainerView.alpha = 0
    bindBottomView(guidesGalleryViewController!)
    view.layoutIfNeeded()
    UIView.animate(withDuration: kDefaultAnimationDuration) { [weak self] in
      self?.guidesVisibleConstraint.priority = .defaultHigh
      self?.guidesHiddenConstraint.priority = .defaultLow
      self?.guidesGalleryContainerView.alpha = 1
      self?.view.layoutIfNeeded()
    }
 }

  private func hideGuidesGallery() {
    guard let guidesGalleryViewController = guidesGalleryViewController else { return }
    view.layoutIfNeeded()
    UIView.animate(withDuration: kDefaultAnimationDuration) { [weak self] in
      self?.guidesVisibleConstraint.priority = .defaultLow
      self?.guidesHiddenConstraint.priority = .defaultHigh
      self?.guidesGalleryContainerView.alpha = 0
      self?.view.layoutIfNeeded()
    } completion: { [weak self] _ in
      guidesGalleryViewController.view.removeFromSuperview()
      guidesGalleryViewController.willMove(toParent: nil)
      guidesGalleryViewController.removeFromParent()
      self?.guidesGalleryViewController = nil
      self?.guidesGalleryContainerView.isHidden = true
      self?.guidesVisibleConstraint.constant = 48
    }
  }

  func showGuidesNavigationBar(_ groupId: MWMMarkGroupID) {
    guidesNavigationBar = GuidesNavigationBarViewController(categoryId: groupId)
    view.addSubview(guidesNavigationBar!.view)
    addChild(guidesNavigationBar!)
    guidesNavigationBar!.didMove(toParent: self)
//    guidesNavigationBar!.configLayout()
    NSLayoutConstraint.activate([
      guidesNavigationBar!.view.topAnchor.constraint(equalTo: view.topAnchor),
      guidesNavigationBar!.view.leftAnchor.constraint(equalTo: view.leftAnchor),
      guidesNavigationBar!.view.rightAnchor.constraint(equalTo: view.rightAnchor)
    ])

//    self.menuState = MWMBottomMenuStateHidden;
  }

  func hideGuidesNavigationBar() {
    guard let guidesNavigationBar = guidesNavigationBar else { return }
    guidesNavigationBar.view.removeFromSuperview()
    guidesNavigationBar.willMove(toParent: nil)
    guidesNavigationBar.removeFromParent()
    self.guidesNavigationBar = nil

//    _guidesNavigationBarHidden = NO;
//    self.menuState = _menuRestoreState;

  }

  private func bindBottomView(_ overlay: MapOverlayViewProtocol) {
    let constraint = mapButtonsView.bottomAnchor.constraint(equalTo: overlay.bottomView.topAnchor)
    constraint.priority = .defaultHigh
    constraint.isActive = true
  }

  @IBAction func onZoomIn(_ sender: UIButton) {
    presenter.zoomIn()
  }

  @IBAction func onZoomOut(_ sender: UIButton) {
    presenter.zoomOut()
  }

  @IBAction func onLocation(_ sender: UIButton) {
    presenter.switchMyPosition()
  }

  @IBAction func onGuidesGalleryPan(_ sender: UIPanGestureRecognizer) {
    let originalConstraint: CGFloat = 48
    switch sender.state {
    case .possible, .began:
      break
    case .changed:
      let dy = sender.translation(in: guidesGalleryContainerView).y
      sender.setTranslation(.zero, in: guidesGalleryContainerView)
      let constant = min(guidesVisibleConstraint.constant - dy, originalConstraint)
      guidesVisibleConstraint.constant = constant
      guidesGalleryContainerView.alpha = (constant + guidesGalleryContainerView.frame.height) / (guidesGalleryContainerView.frame.height + originalConstraint)
    case .ended, .cancelled, .failed:
      let velocity = sender.velocity(in: guidesGalleryContainerView).y
      if velocity < 0 || (velocity == 0 && guidesGalleryContainerView.alpha > 0.8) {
        view.layoutIfNeeded()
        UIView.animate(withDuration: kDefaultAnimationDuration) { [weak self] in
          self?.guidesVisibleConstraint.constant = originalConstraint
          self?.guidesGalleryContainerView.alpha = 1
          self?.view.layoutIfNeeded()
        }
      } else {
        FrameworkHelper.deactivateMapSelection(notifyUI: true)
      }
    @unknown default:
      fatalError()
    }
  }
}

extension MapControlsViewController: IMapControlsView {
  func showMenu(_ mode: MapControlsMenuMode) {
    switch mode {
    case .full:
      guard menuViewController == nil else { fatalError() }
      menuViewController = BottomMenuBuilder.buildMenu(mapViewController: MapViewController.shared(), delegate: self)
      present(menuViewController!, animated: true)
    case .layers:
      guard menuViewController == nil else { fatalError() }
      menuViewController = BottomMenuBuilder.buildLayers(mapViewController: MapViewController.shared(), delegate: self)
      present(menuViewController!, animated: true)
    case .inactive:
      guard menuViewController != nil else { fatalError() }
      dismiss(animated: true)
      menuViewController = nil
    case .hidden:
      guard menuViewController != nil else { fatalError() }
      dismiss(animated: true)
      menuViewController = nil
    }
  }

  func showPlacePage() {
    guard PlacePageData.hasData else { return }

    if PlacePageData.isGuide {
      showGuidesGallery()
    } else {
      showRegularPlacePage()
    }
  }

  func hidePlacePage() {
    guard let placePageViewController = placePageViewController else {
      hideGuidesGallery()
      return
    }
    placePageViewController.view.removeFromSuperview()
    placePageViewController.willMove(toParent: nil)
    placePageViewController.removeFromParent()
    self.placePageViewController = nil
    placePageContainerView.isHidden = true
  }

  func updatePlacePage() {

  }

  func setSideButtonsVisible(_ visible: Bool) {
    view.layoutIfNeeded()
    UIView.animate(withDuration: kDefaultAnimationDuration) { [weak self] in
      guard let self = self else { return }
      if visible {
        self.sideButtonsContainerView.alpha = 1
        self.sideButtonsVisibleConstraint.priority = .defaultHigh
        self.sideButtonsHiddenConstraint.priority = .defaultLow
      } else {
        self.sideButtonsContainerView.alpha = 0
        self.sideButtonsVisibleConstraint.priority = .defaultLow
        self.sideButtonsHiddenConstraint.priority = .defaultHigh
      }
      self.view.layoutIfNeeded()
    }
  }

  func setLayersButtonVisible(_ visible: Bool) {
    view.layoutIfNeeded()
    UIView.animate(withDuration: kDefaultAnimationDuration) { [weak self] in
      guard let self = self else { return }
      if visible {
        self.layersButtonContainerView.alpha = 1
        self.layersButtonVisibleConstraint.priority = .defaultHigh
        self.layersButtonHiddenConstraint.priority = .defaultLow
      } else {
        self.layersButtonContainerView.alpha = 0
        self.layersButtonVisibleConstraint.priority = .defaultLow
        self.layersButtonHiddenConstraint.priority = .defaultHigh
      }
      self.view.layoutIfNeeded()
    }
  }

  func showTutorialType(_ tutorialType: TipType) {
    switch tutorialType {
    case .bookmarks:
      tutorialViewController = TutorialViewController.tutorial(.bookmarks,
                                                               target: tabBarViewController.bookmarksButton,
                                                               delegate: self)
    case .search:
      tutorialViewController = TutorialViewController.tutorial(.search,
                                                               target: tabBarViewController.searchButton,
                                                               delegate: self)

    case .discovery:
      tutorialViewController = TutorialViewController.tutorial(.discovery,
                                                               target: tabBarViewController.discoveryButton,
                                                               delegate: self)

    case .subway:
      tutorialViewController = TutorialViewController.tutorial(.subway,
                                                               target: trafficButtonViewController.view as! UIControl,
                                                               delegate: self)

    case .isolines:
      tutorialViewController = TutorialViewController.tutorial(.isolines,
                                                               target: trafficButtonViewController.view as! UIControl,
                                                               delegate: self)

    case .none:
      tutorialViewController = nil
    @unknown default:
      fatalError()
    }

    guard let tutorialViewController = tutorialViewController else { return }

    addChild(tutorialViewController)
    view.addSubview(tutorialViewController.view)
    tutorialViewController.view.alignToSuperview()
    tutorialViewController.didMove(toParent: self)
  }

  func hideTutorial() {
    guard let tutorialViewController = tutorialViewController else { return }
    tutorialViewController.fadeOut {
      tutorialViewController.view.removeFromSuperview()
      tutorialViewController.willMove(toParent: nil)
      tutorialViewController.removeFromParent()
      self.tutorialViewController = nil
    }
  }

  func showAfterBoooking(_ data: PromoAfterBookingData) {
    let alert = PromoAfterBookingViewController(cityImageUrl: data.pictureUrl) { [weak self] in
      self?.dismiss(animated: true)
      self?.presenter.onAfterBooking()
    } cancelClosure: { [weak self] in
      self?.dismiss(animated: true)
    }
    present(alert, animated: true)
  }
}

extension MapControlsViewController: BottomTabBarViewControllerDelegate {
  func search() {
    presenter.search()
  }

  func route() {
    presenter.route()
  }

  func discovery() {
    presenter.discovery()
  }

  func bookmarks() {
    presenter.bookmarks()
  }

  func menu() {
    presenter.menu()
  }
}

extension MapControlsViewController: BottomMenuDelegate {
  func addPlace() {

  }

  func didFinishAddingPlace() {

  }

  func didFinish() {
    presenter.hideMenu()
  }
}

extension MapControlsViewController: TutorialViewControllerDelegate {
  func didPressTarget(_ viewController: TutorialViewController) {
    presenter.onTutorialTarget()
  }

  func didPressCancel(_ viewController: TutorialViewController) {
    presenter.onTutorialCancel()
  }

  func didPressOnScreen(_ viewController: TutorialViewController) {
    presenter.onTutorialScreenTap()
  }
}
