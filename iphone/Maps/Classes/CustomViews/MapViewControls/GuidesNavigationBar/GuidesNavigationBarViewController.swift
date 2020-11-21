class GuidesNavigationBarViewController: UIViewController {
  private var availableArea = CGRect.zero
  private var mapViewController = MapViewController.shared()
  private var category: BookmarkGroup

  @IBOutlet var navigationBar: UINavigationBar!
  @IBOutlet var navigationBarItem: UINavigationItem!

  init(categoryId: MWMMarkGroupID) {
    category = BookmarksManager.shared().category(withId: categoryId)
    super.init(nibName: nil, bundle: nil)
  }

  required init?(coder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }

  override func viewDidLoad() {
    super.viewDidLoad()
    navigationBarItem.title = category.title

    let leftButton: UIButton = UIButton(type: .custom)
    leftButton.setImage(UIImage(named: "ic_nav_bar_back")?.withRenderingMode(.alwaysTemplate), for: .normal)
    leftButton.setTitle(L("bookmarks_groups"), for: .normal)
    leftButton.addTarget(self, action: #selector(onBackPressed), for: .touchUpInside)
    leftButton.contentHorizontalAlignment = .left
    leftButton.styleName = "FlatNormalTransButton"

    navigationBarItem.leftBarButtonItem = UIBarButtonItem(customView: leftButton)
    navigationBarItem.rightBarButtonItem = UIBarButtonItem(title: L("cancel"),
                                                           style: .plain,
                                                           target: self,
                                                           action: #selector(onCancelPessed))
  }

  @objc func onBackPressed(_ sender: Any) {
    MapViewController.shared()?.bookmarksCoordinator.open()
    Statistics.logEvent(kStatBackClick, withParameters: [kStatFrom: kStatMap,
                                                         kStatTo: kStatBookmarks])
  }

  @objc func onCancelPessed(_ sender: Any) {
    MapViewController.shared()?.bookmarksCoordinator.close()
  }
}
