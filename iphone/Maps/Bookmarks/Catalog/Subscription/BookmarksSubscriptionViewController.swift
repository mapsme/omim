import SafariServices

@objc class BookmarksSubscriptionViewController: BaseSubscriptionViewController {
  //MARK: outlets
  @IBOutlet private var annualSubscriptionButton: BookmarksSubscriptionButton!
  @IBOutlet private var annualDiscountLabel: BookmarksSubscriptionDiscountLabel!
  @IBOutlet private var monthlySubscriptionButton: BookmarksSubscriptionButton!
  
  override var subscriptionManager: ISubscriptionManager? {
    get { return InAppPurchase.bookmarksSubscriptionManager }
  }

  override var supportedInterfaceOrientations: UIInterfaceOrientationMask {
    get { return [.portrait] }
  }

  override var preferredStatusBarStyle: UIStatusBarStyle {
    get { return UIColor.isNightMode() ? .lightContent : .default }
  }

  override init(nibName nibNameOrNil: String?, bundle nibBundleOrNil: Bundle?) {
    super.init(nibName: nibNameOrNil, bundle: nibBundleOrNil)
  }

  required init?(coder aDecoder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }

  override func viewDidLoad() {
    super.viewDidLoad()

    annualSubscriptionButton.config(title: L("annual_subscription_title"),
                                    price: "...",
                                    enabled: false)
    monthlySubscriptionButton.config(title: L("montly_subscription_title"),
                                     price: "...",
                                     enabled: false)
    
    if !UIColor.isNightMode() {
      annualDiscountLabel.layer.shadowRadius = 4
      annualDiscountLabel.layer.shadowOffset = CGSize(width: 0, height: 2)
      annualDiscountLabel.layer.shadowColor = UIColor.blackHintText().cgColor
      annualDiscountLabel.layer.shadowOpacity = 0.62
    }
    annualDiscountLabel.layer.cornerRadius = 6
    annualDiscountLabel.isHidden = true

    self.configure(buttons: [
      .year: annualSubscriptionButton,
      .month: monthlySubscriptionButton],
                   discountLabels:[
      .year: annualDiscountLabel])

    Statistics.logEvent(kStatInappShow, withParameters: [kStatVendor: MWMPurchaseManager.bookmarksSubscriptionVendorId(),
                                                         kStatPurchase: MWMPurchaseManager.bookmarksSubscriptionServerId(),
                                                         kStatProduct: BOOKMARKS_SUBSCRIPTION_YEARLY_PRODUCT_ID,
                                                         kStatFrom: source], with: .realtime)
  }

  @IBAction func onAnnualButtonTap(_ sender: UIButton) {
    purchase(sender: sender, period: .year)
  }

  @IBAction func onMonthlyButtonTap(_ sender: UIButton) {
    purchase(sender: sender, period: .month)
  }
}
