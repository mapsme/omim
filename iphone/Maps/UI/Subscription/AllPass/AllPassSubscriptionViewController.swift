class AllPassSubscriptionViewController: UIViewController {
  var presenter: SubscriptionPresenterProtocol!

  @IBOutlet private var backgroundImageView: ImageViewCrossDisolve!
  @IBOutlet private var annualSubscriptionButton: BookmarksSubscriptionButton!
  @IBOutlet private var annualDiscountLabel: InsetsLabel!
  @IBOutlet private var monthlySubscriptionButton: BookmarksSubscriptionButton!
  @IBOutlet private var trialSubscriptionButton: UIButton!
  @IBOutlet private var trialSubscriptionLabel: UILabel!
  @IBOutlet private var descriptionPageScrollView: UIScrollView!
  @IBOutlet private var contentView: UIView!
  @IBOutlet private var statusBarBackgroundView: UIVisualEffectView!
  @IBOutlet private var descriptionSubtitles: [UILabel]!
  @IBOutlet private var loadingView: UIView!
  @IBOutlet private var stackTopOffset: NSLayoutConstraint!
  @IBOutlet private var loadingStateActivityIndicator: UIActivityIndicatorView!

  override var supportedInterfaceOrientations: UIInterfaceOrientationMask { return [.portrait] }
  override var preferredStatusBarStyle: UIStatusBarStyle { return .lightContent }
  private let transitioning = FadeTransitioning<IPadModalPresentationController>()

  private var pageWidth: CGFloat {
    return descriptionPageScrollView.frame.width
  }

  private let maxPages = 3
  private var currentPage: Int {
    return Int(descriptionPageScrollView.contentOffset.x / pageWidth) + 1
  }

  private var animatingTask: DispatchWorkItem?
  private let animationDelay: TimeInterval = 2
  private let animationDuration: TimeInterval = 0.75
  private let animationBackDuration: TimeInterval = 0.3
  private let statusBarBackVisibleThreshold: CGFloat = 60
  private let stackTopOffsetSubscriptions: CGFloat = 60
  private let stackTopOffsetTrial: CGFloat = 48

  override init(nibName nibNameOrNil: String?, bundle nibBundleOrNil: Bundle?) {
    super.init(nibName: nibNameOrNil, bundle: nibBundleOrNil)
    if UIDevice.current.userInterfaceIdiom == .pad {
      transitioningDelegate = transitioning
      modalPresentationStyle = .custom
    } else {
      modalPresentationStyle = .fullScreen
    }
  }

  required init?(coder aDecoder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }

  override func viewDidLoad() {
    super.viewDidLoad()
    presenter?.configure()

    backgroundImageView.images = [
      UIImage(named: "AllPassSubscriptionBg1"),
      UIImage(named: "AllPassSubscriptionBg2"),
      UIImage(named: "AllPassSubscriptionBg3"),
    ]
    startAnimating()

    let fontSize: CGFloat = UIScreen.main.bounds.width > 320 ? 17.0 : 14.0
    let bold = UIFont.systemFont(ofSize: fontSize, weight: .heavy + 100)
    zip(descriptionSubtitles, ["all_pass_subscription_message_subtitle",
                               "all_pass_subscription_message_subtitle_3",
                               "all_pass_subscription_message_subtitle_2"]).forEach { title, loc in
                                title.attributedText = NSMutableAttributedString(htmlString: L(loc), baseFont: bold)
                               }

    preferredContentSize = CGSize(width: 414, height: contentView.frame.height)
  }

  @IBAction func onAnnualButtonTap(_ sender: UIButton) {
    presenter.purchase(anchor: sender, period: .year)
  }

  @IBAction func onMonthlyButtonTap(_ sender: UIButton) {
    presenter.purchase(anchor: sender, period: .month)
  }

  @IBAction func onClose(_ sender: UIButton) {
    presenter.onClose()
  }

  @IBAction func onTerms(_ sender: UIButton) {
    presenter.onTermsPressed()
  }

  @IBAction func onPrivacy(_ sender: UIButton) {
    presenter.onPrivacyPressed()
  }

  @IBAction func onRestore(sender: UIButton) {
    presenter.restore(anchor: sender)
  }

  @IBAction func onTrial(sender: UIButton) {
    presenter.trial(anchor: sender)
  }
}

extension AllPassSubscriptionViewController: SubscriptionViewProtocol {
  var isLoadingHidden: Bool {
    get {
      return loadingView.isHidden
    }
    set {
      loadingView.isHidden = newValue
    }
  }

  func setModel(_ model: SubscriptionViewModel) {
    annualSubscriptionButton.isHidden = true
    monthlySubscriptionButton.isHidden = true
    trialSubscriptionButton.isHidden = true
    trialSubscriptionLabel.isHidden = true
    annualDiscountLabel.isHidden = true
    loadingStateActivityIndicator.isHidden = true
    switch model {
    case .loading:
      loadingStateActivityIndicator.isHidden = false
      stackTopOffset.constant = stackTopOffsetSubscriptions
    case let .subsctiption(subscriptionData):
      annualSubscriptionButton.isHidden = false
      monthlySubscriptionButton.isHidden = false
      stackTopOffset.constant = stackTopOffsetSubscriptions
      for data in subscriptionData {
        if data.period == .month {
          monthlySubscriptionButton.config(title: data.title,
                                           price: data.price,
                                           enabled: true)
        }
        if data.period == .year {
          annualSubscriptionButton.config(title: data.title,
                                          price: data.price,
                                          enabled: true)
          annualDiscountLabel.isHidden = !data.hasDiscount
          annualDiscountLabel.text = data.discount
        }
      }
    case let .trial(trialData):
      trialSubscriptionButton.isHidden = false
      trialSubscriptionLabel.isHidden = false
      stackTopOffset.constant = stackTopOffsetTrial
      trialSubscriptionLabel.text = String(coreFormat: L("guides_trial_message"), arguments: [trialData.price])
    }
  }
}

extension AllPassSubscriptionViewController: UIAdaptivePresentationControllerDelegate {
  func presentationControllerDidDismiss(_ presentationController: UIPresentationController) {
    presenter.onCancel()
  }
}

// MARK: Animation

extension AllPassSubscriptionViewController {
  private func perform(withDelay: TimeInterval, execute: DispatchWorkItem?) {
    if let execute = execute {
      DispatchQueue.main.asyncAfter(deadline: .now() + withDelay, execute: execute)
    }
  }

  private func startAnimating() {
    if animatingTask != nil {
      animatingTask?.cancel()
    }
    animatingTask = DispatchWorkItem.init { [weak self, animationDelay] in
      self?.scrollToWithAnimation(page: (self?.currentPage ?? 0) + 1, completion: {
        self?.perform(withDelay: animationDelay, execute: self?.animatingTask)
      })
    }
    perform(withDelay: animationDelay, execute: animatingTask)
  }

  private func stopAnimating() {
    animatingTask?.cancel()
    animatingTask = nil
    view.layer.removeAllAnimations()
  }

  private func scrollToWithAnimation(page: Int, completion: @escaping () -> Void) {
    var nextPage = page
    var duration = animationDuration
    if nextPage < 1 || nextPage > maxPages {
      nextPage = 1
      duration = animationBackDuration
    }

    let xOffset = CGFloat(nextPage - 1) * pageWidth
    UIView.animate(withDuration: duration,
                   delay: 0,
                   options: [.curveEaseInOut, .allowUserInteraction],
                   animations: { [weak self] in
                    self?.descriptionPageScrollView.contentOffset.x = xOffset
      }, completion: { complete in
        completion()
    })
  }
}

extension AllPassSubscriptionViewController: UIScrollViewDelegate {
  func scrollViewDidScroll(_ scrollView: UIScrollView) {
    if scrollView == descriptionPageScrollView {
      let pageProgress = scrollView.contentOffset.x / pageWidth
      backgroundImageView.currentPage = pageProgress
    } else {
      let statusBarAlpha = min(scrollView.contentOffset.y / statusBarBackVisibleThreshold, 1)
      statusBarBackgroundView.alpha = statusBarAlpha
    }
  }

  func scrollViewWillBeginDragging(_ scrollView: UIScrollView) {
    stopAnimating()
  }

  func scrollViewDidEndDragging(_ scrollView: UIScrollView, willDecelerate decelerate: Bool) {
    startAnimating()
  }
}
