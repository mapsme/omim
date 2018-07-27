fileprivate class ContentCell: UICollectionViewCell {
  var view: UIView? {
    didSet {
      if let view = view, view != oldValue {
        oldValue?.removeFromSuperview()
        view.frame = contentView.bounds
        contentView.addSubview(view)
      }
    }
  }

  override func layoutSubviews() {
    super.layoutSubviews()
    if let view = view {
      view.frame = contentView.bounds
    }
  }
}

fileprivate class HeaderCell: UICollectionViewCell {
  private let label = UILabel()

  override init(frame: CGRect) {
    super.init(frame: frame)
    contentView.addSubview(label)
    label.textAlignment = .center
  }

  required init?(coder aDecoder: NSCoder) {
    super.init(coder: aDecoder)
    contentView.addSubview(label)
    label.textAlignment = .center
  }

  var attributedText: NSAttributedString? {
    didSet {
      label.attributedText = attributedText
    }
  }

  override func prepareForReuse() {
    super.prepareForReuse()
    attributedText = nil
  }

  override func layoutSubviews() {
    super.layoutSubviews()
    label.frame = contentView.bounds
  }
}

protocol TabViewDataSource: AnyObject {
  func numberOfPages(in tabView: TabView) -> Int
  func tabView(_ tabView: TabView, viewAt index: Int) -> UIView
  func tabView(_ tabView: TabView, titleAt index: Int) -> String?
}

protocol TabViewDelegate: AnyObject {
  func tabView(_ tabView: TabView, didSelectTabAt index: Int)
}

@objcMembers
@objc(MWMTabView)
class TabView: UIView {
  private enum CellId {
    static let content = "contentCell"
    static let header = "headerCell"
  }

  private let tabsLayout = UICollectionViewFlowLayout()
  private let tabsContentLayout = UICollectionViewFlowLayout()
  private let tabsCollectionView: UICollectionView
  private let tabsContentCollectionView: UICollectionView
  private let headerView = UIView()
  private let slidingView = UIView()
  private var slidingViewLeft: NSLayoutConstraint!
  private var slidingViewWidth: NSLayoutConstraint!
  private lazy var pageCount = { return self.dataSource?.numberOfPages(in: self) ?? 0; }()
  var selectedIndex: Int?
  private var lastSelectedIndex: Int?

  weak var dataSource: TabViewDataSource?
  weak var delegate: TabViewDelegate?

  var barTintColor = UIColor.white {
    didSet {
      headerView.backgroundColor = barTintColor
    }
  }

  var headerTextAttributes: [NSAttributedStringKey : Any] = [
    .foregroundColor : UIColor.white,
    .font : UIFont.systemFont(ofSize: 14, weight: .semibold)
    ] {
    didSet {
      tabsCollectionView.reloadData()
    }
  }

  var contentFrame: CGRect {
    if #available(iOS 11.0, *) {
      return safeAreaLayoutGuide.layoutFrame
    } else {
      return bounds
    }
  }

  override var tintColor: UIColor! {
    didSet {
      slidingView.backgroundColor = tintColor
    }
  }

  override init(frame: CGRect) {
    tabsCollectionView = UICollectionView(frame: .zero, collectionViewLayout: tabsLayout)
    tabsContentCollectionView = UICollectionView(frame: .zero, collectionViewLayout: tabsContentLayout)
    super.init(frame: frame)
    configure()
  }

  required init?(coder aDecoder: NSCoder) {
    tabsCollectionView = UICollectionView(frame: .zero, collectionViewLayout: tabsLayout)
    tabsContentCollectionView = UICollectionView(frame: .zero, collectionViewLayout: tabsContentLayout)
    super.init(coder: aDecoder)
    configure()
  }

  private func configure() {
    backgroundColor = .white

    configureHeader()
    configureContent()
    
    addSubview(tabsContentCollectionView)
    addSubview(headerView)

    configureLayoutContraints()
  }

  private func configureHeader() {
    tabsLayout.scrollDirection = .horizontal
    tabsLayout.minimumLineSpacing = 0
    tabsLayout.minimumInteritemSpacing = 0

    tabsCollectionView.register(HeaderCell.self, forCellWithReuseIdentifier: CellId.header)
    tabsCollectionView.dataSource = self
    tabsCollectionView.delegate = self
    tabsCollectionView.backgroundColor = .clear

    slidingView.backgroundColor = tintColor

    headerView.layer.shadowOffset = CGSize(width: 0, height: 2)
    headerView.layer.shadowColor = UIColor(white: 0, alpha: 1).cgColor
    headerView.layer.shadowOpacity = 0.12
    headerView.layer.shadowRadius = 2
    headerView.layer.masksToBounds = false
    headerView.backgroundColor = barTintColor
    headerView.addSubview(tabsCollectionView)
    headerView.addSubview(slidingView)
  }

  private func configureContent() {
    tabsContentLayout.scrollDirection = .horizontal
    tabsContentLayout.minimumLineSpacing = 0
    tabsContentLayout.minimumInteritemSpacing = 0

    tabsContentCollectionView.register(ContentCell.self, forCellWithReuseIdentifier: CellId.content)
    tabsContentCollectionView.dataSource = self
    tabsContentCollectionView.delegate = self
    tabsContentCollectionView.isPagingEnabled = true
    tabsContentCollectionView.bounces = false
    tabsContentCollectionView.showsVerticalScrollIndicator = false
    tabsContentCollectionView.showsHorizontalScrollIndicator = false
    tabsContentCollectionView.backgroundColor = .clear
  }

  private func configureLayoutContraints() {
    tabsCollectionView.translatesAutoresizingMaskIntoConstraints = false;
    tabsContentCollectionView.translatesAutoresizingMaskIntoConstraints = false
    headerView.translatesAutoresizingMaskIntoConstraints = false
    slidingView.translatesAutoresizingMaskIntoConstraints = false

    let top: NSLayoutYAxisAnchor
    let bottom: NSLayoutYAxisAnchor
    let left: NSLayoutXAxisAnchor
    let right: NSLayoutXAxisAnchor
    if #available(iOS 11.0  , *) {
      top = safeAreaLayoutGuide.topAnchor
      bottom = safeAreaLayoutGuide.bottomAnchor
      left = safeAreaLayoutGuide.leftAnchor
      right = safeAreaLayoutGuide.rightAnchor
    } else {
      top = topAnchor
      bottom = bottomAnchor
      left = leftAnchor
      right = rightAnchor
    }

    headerView.leftAnchor.constraint(equalTo: leftAnchor).isActive = true
    headerView.rightAnchor.constraint(equalTo: rightAnchor).isActive = true
    headerView.topAnchor.constraint(equalTo: top).isActive = true
    headerView.heightAnchor.constraint(equalToConstant: 46).isActive = true

    tabsContentCollectionView.leftAnchor.constraint(equalTo: left).isActive = true
    tabsContentCollectionView.rightAnchor.constraint(equalTo: right).isActive = true
    tabsContentCollectionView.topAnchor.constraint(equalTo: headerView.bottomAnchor).isActive = true
    tabsContentCollectionView.bottomAnchor.constraint(equalTo: bottom).isActive = true

    tabsCollectionView.leftAnchor.constraint(equalTo: headerView.leftAnchor).isActive = true
    tabsCollectionView.rightAnchor.constraint(equalTo: headerView.rightAnchor).isActive = true
    tabsCollectionView.topAnchor.constraint(equalTo: headerView.topAnchor).isActive = true
    tabsCollectionView.bottomAnchor.constraint(equalTo: slidingView.topAnchor).isActive = true

    slidingView.heightAnchor.constraint(equalToConstant: 3).isActive = true
    slidingView.bottomAnchor.constraint(equalTo: headerView.bottomAnchor).isActive = true

    slidingViewLeft = slidingView.leftAnchor.constraint(equalTo: left)
    slidingViewLeft.isActive = true
    slidingViewWidth = slidingView.widthAnchor.constraint(equalToConstant: 0)
    slidingViewWidth.isActive = true
  }

  override func layoutSubviews() {
    tabsLayout.invalidateLayout()
    tabsContentLayout.invalidateLayout()
    super.layoutSubviews()
    assert(pageCount > 0)
    slidingViewWidth.constant = pageCount > 0 ? contentFrame.width / CGFloat(pageCount) : 0
    slidingViewLeft.constant = pageCount > 0 ? contentFrame.width / CGFloat(pageCount) * CGFloat(selectedIndex ?? 0) : 0
    tabsContentCollectionView.layoutIfNeeded()
    if let selectedIndex = selectedIndex {
      tabsContentCollectionView.scrollToItem(at: IndexPath(item: selectedIndex, section: 0),
                                             at: .left,
                                             animated: false)
    }
  }
}

extension TabView : UICollectionViewDataSource {
  func collectionView(_ collectionView: UICollectionView, numberOfItemsInSection section: Int) -> Int {
    return pageCount
  }

  func collectionView(_ collectionView: UICollectionView, cellForItemAt indexPath: IndexPath) -> UICollectionViewCell {
    var cell = UICollectionViewCell()
    if collectionView == tabsContentCollectionView {
      cell = collectionView.dequeueReusableCell(withReuseIdentifier: CellId.content, for: indexPath)
      if let contentCell = cell as? ContentCell {
        contentCell.view = dataSource?.tabView(self, viewAt: indexPath.item)
      }
    }

    if collectionView == tabsCollectionView {
      cell = collectionView.dequeueReusableCell(withReuseIdentifier: CellId.header, for: indexPath)
      if let headerCell = cell as? HeaderCell {
        let title = dataSource?.tabView(self, titleAt: indexPath.item) ?? ""
        headerCell.attributedText = NSAttributedString(string: title.uppercased(), attributes: headerTextAttributes)
      }
    }

    return cell
  }
}

extension TabView : UICollectionViewDelegateFlowLayout {
  func scrollViewDidScroll(_ scrollView: UIScrollView) {
    if scrollView.contentSize.width > 0 {
      let scrollOffset = scrollView.contentOffset.x / scrollView.contentSize.width
      slidingViewLeft.constant = scrollOffset * contentFrame.width
    }
  }

  func scrollViewWillBeginDragging(_ scrollView: UIScrollView) {
    lastSelectedIndex = selectedIndex
  }

  func scrollViewDidEndDecelerating(_ scrollView: UIScrollView) {
    selectedIndex = Int(round(scrollView.contentOffset.x / scrollView.bounds.width))
    if let selectedIndex = selectedIndex, selectedIndex != lastSelectedIndex {
      delegate?.tabView(self, didSelectTabAt: selectedIndex)
    }
  }

  func collectionView(_ collectionView: UICollectionView, didSelectItemAt indexPath: IndexPath) {
    if (collectionView == tabsCollectionView) {
      if selectedIndex != indexPath.item {
        selectedIndex = indexPath.item
        tabsContentCollectionView.scrollToItem(at: indexPath, at: .left, animated: true)
        delegate?.tabView(self, didSelectTabAt: selectedIndex!)
      }
    }
  }

  func collectionView(_ collectionView: UICollectionView,
                      layout collectionViewLayout: UICollectionViewLayout,
                      sizeForItemAt indexPath: IndexPath) -> CGSize {
    var bounds = collectionView.bounds
    if #available(iOS 11.0, *) {
      bounds = UIEdgeInsetsInsetRect(bounds, collectionView.adjustedContentInset)
    }

    if collectionView == tabsContentCollectionView {
      return bounds.size
    } else {
      return CGSize(width: bounds.width / CGFloat(pageCount),
                    height: bounds.height)
    }
  }
}
