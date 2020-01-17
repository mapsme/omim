struct CatalogCategoryInfo {
  var id: String
  var name: String
  var author: String?
  var productId: String?
  var imageUrl: String?
  var subscriptionType: SubscriptionGroupType

  init?(_ components: [String : String], type: SubscriptionGroupType) {
    guard let id = components["id"],
      let name = components["name"] else { return nil }
    self.id = id
    self.name = name
    author = components["author_name"]
    productId = components["tier"]
    imageUrl = components["img"]
    subscriptionType = type
  }
}

@objc(MWMCatalogWebViewController)
final class CatalogWebViewController: WebViewController {
  let progressBgView = UIVisualEffectView(effect:
    UIBlurEffect(style: UIColor.isNightMode() ? .light : .dark))
  let progressView = ActivityIndicator()
  let numberOfTasksLabel = UILabel()
  let loadingIndicator = UIActivityIndicatorView(style: .gray)
  let pendingTransactionsHandler = InAppPurchase.pendingTransactionsHandler()
  var deeplink: URL?
  var categoryInfo: CatalogCategoryInfo?
  var statSent = false
  var backButton: UIBarButtonItem!
  var billing = InAppPurchase.inAppBilling()
  var noInternetView: CatalogConnectionErrorView!

  @objc static func catalogFromAbsoluteUrl(_ url: URL? = nil, utm: MWMUTM = .none) -> CatalogWebViewController {
    return CatalogWebViewController(url, utm:utm, isAbsoluteUrl:true)
  }
  
  @objc static func catalogFromDeeplink(_ url: URL, utm: MWMUTM = .none) -> CatalogWebViewController {
    return CatalogWebViewController(url, utm:utm)
  }

  private init(_ url: URL? = nil, utm: MWMUTM = .none, isAbsoluteUrl: Bool = false) {
    var catalogUrl = MWMBookmarksManager.shared().catalogFrontendUrl(utm)!
    if let u = url {
      if isAbsoluteUrl {
        catalogUrl = u
      } else {
        if u.host == "guides_page" {
          if let urlComponents = URLComponents(url: u, resolvingAgainstBaseURL: false),
            let path = urlComponents.queryItems?.reduce(into: "", { if $1.name == "url" { $0 = $1.value } }),
            let calculatedUrl = MWMBookmarksManager.shared().catalogFrontendUrlPlusPath(path, utm: utm) {
            catalogUrl = calculatedUrl
          }
        } else {
          deeplink = url
        }
        Statistics.logEvent(kStatCatalogOpen, withParameters: [kStatFrom : kStatDeeplink])
      }
    }
    super.init(url: catalogUrl, title: L("guides_catalogue_title"))!
    let bButton = UIButton(type: .custom)
    bButton.addTarget(self, action: #selector(onBack), for: .touchUpInside)
    bButton.setImage(UIImage(named: "ic_nav_bar_back"), for: .normal)
    backButton = UIBarButtonItem(customView: bButton)
    noInternetView = CatalogConnectionErrorView(frame: .zero, actionCallback: { [weak self] in
      guard let self = self else { return }
      if !FrameworkHelper.isNetworkConnected() {
        self.noInternetView.isHidden = false
        return
      }

      self.noInternetView.isHidden = true
      self.loadingIndicator.startAnimating()
      if self.webView.url != nil {
        self.webView.reload()
      } else {
        self.performURLRequest()
      }
    })
  }

  required init?(coder aDecoder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }

  override func viewDidLoad() {
    super.viewDidLoad()

    numberOfTasksLabel.translatesAutoresizingMaskIntoConstraints = false
    progressView.translatesAutoresizingMaskIntoConstraints = false
    progressBgView.translatesAutoresizingMaskIntoConstraints = false
    loadingIndicator.translatesAutoresizingMaskIntoConstraints = false
    numberOfTasksLabel.styleName = "medium14:whiteText"
    numberOfTasksLabel.text = "0"
    progressBgView.layer.cornerRadius = 8
    progressBgView.clipsToBounds = true
    progressBgView.contentView.addSubview(progressView)
    progressBgView.contentView.addSubview(numberOfTasksLabel)
    view.addSubview(progressBgView)
    loadingIndicator.startAnimating()
    view.addSubview(loadingIndicator)

    loadingIndicator.centerXAnchor.constraint(equalTo: view.centerXAnchor).isActive = true
    loadingIndicator.centerYAnchor.constraint(equalTo: view.centerYAnchor).isActive = true
    numberOfTasksLabel.centerXAnchor.constraint(equalTo: progressBgView.centerXAnchor).isActive = true
    numberOfTasksLabel.centerYAnchor.constraint(equalTo: progressBgView.centerYAnchor).isActive = true
    progressView.centerXAnchor.constraint(equalTo: progressBgView.centerXAnchor).isActive = true
    progressView.centerYAnchor.constraint(equalTo: progressBgView.centerYAnchor).isActive = true
    progressBgView.widthAnchor.constraint(equalToConstant: 48).isActive = true
    progressBgView.heightAnchor.constraint(equalToConstant: 48).isActive = true

    let connected = FrameworkHelper.isNetworkConnected()
    if !connected {
      Statistics.logEvent("Bookmarks_Downloaded_Catalogue_error",
                          withParameters: [kStatError : "no_internet"])
    }

    noInternetView.isHidden = connected
    noInternetView.translatesAutoresizingMaskIntoConstraints = false
    view.addSubview(noInternetView)
    noInternetView.centerXAnchor.constraint(equalTo: view.centerXAnchor).isActive = true
    noInternetView.centerYAnchor.constraint(equalTo: view.centerYAnchor, constant: -20.0).isActive = true

    if #available(iOS 11, *) {
      progressBgView.leftAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leftAnchor, constant: 8).isActive = true
    } else {
      progressBgView.leftAnchor.constraint(equalTo: view.leftAnchor, constant: 8).isActive = true
    }

    progressView.styleName = "MWMWhite"
    self.view.styleName = "Background"
    
    updateProgress()
    navigationItem.hidesBackButton = true;
    navigationItem.rightBarButtonItem = UIBarButtonItem(title: L("core_exit"), style: .plain, target: self, action:  #selector(goBack))
  }

  override func viewDidAppear(_ animated: Bool) {
    super.viewDidAppear(animated)
    if let deeplink = deeplink {
      processDeeplink(deeplink)
    }
  }

  override func willLoadUrl(_ decisionHandler: @escaping (Bool, Dictionary<String, String>?) -> Void) {
    buildHeaders { [weak self] (headers) in
      self?.handlePendingTransactions {
        decisionHandler($0, headers)
        self?.checkInvalidSubscription()
      }
    }
  }

  override func shouldAddAccessToken() -> Bool {
    return true
  }

  override func webView(_ webView: WKWebView,
                        decidePolicyFor navigationAction: WKNavigationAction,
                        decisionHandler: @escaping (WKNavigationActionPolicy) -> Void) {
    let subscribePath = "subscribe"
    guard let url = navigationAction.request.url,
      url.scheme == "mapsme" || url.path.contains("buy_kml") || url.path.contains(subscribePath) else {
        super.webView(webView, decidePolicyFor: navigationAction, decisionHandler: decisionHandler)
        return
    }

    if url.path.contains(subscribePath) {
      showSubscribe(type: SubscriptionGroupType(catalogURL: url))
      decisionHandler(.cancel);
      return
    }

    processDeeplink(url)
    decisionHandler(.cancel);
  }

  override func webView(_ webView: WKWebView, didCommit navigation: WKNavigation!) {
    if !statSent {
      statSent = true
      MWMEye.boomarksCatalogShown()
    }
    loadingIndicator.stopAnimating()
    if (webView.canGoBack) {
      navigationItem.leftBarButtonItem = backButton
    } else {
      navigationItem.leftBarButtonItem = nil
    }
  }

  override func webView(_ webView: WKWebView, didFail navigation: WKNavigation!, withError error: Error) {
    loadingIndicator.stopAnimating()
    Statistics.logEvent("Bookmarks_Downloaded_Catalogue_error",
                        withParameters: [kStatError : kStatUnknown])
  }

  override func webView(_ webView: WKWebView,
                        didFailProvisionalNavigation navigation: WKNavigation!,
                        withError error: Error) {
    loadingIndicator.stopAnimating()
    Statistics.logEvent("Bookmarks_Downloaded_Catalogue_error",
                        withParameters: [kStatError : kStatUnknown])
  }

  private func showSubscribe(type: SubscriptionGroupType) {
    let subscribeViewController = SubscriptionViewBuilder.build(type: type,
                                                                parentViewController: self,
                                                                source: kStatWebView,
                                                                successDialog: .success) { [weak self] (success) in
                                                                  if (success) {
                                                                    self?.webView.reloadFromOrigin()
                                                                  }
    }
    present(subscribeViewController, animated: true)
  }

  private func buildHeaders(completion: @escaping ([String : String]?) -> Void) {
    billing.requestProducts(Set(MWMPurchaseManager.bookmarkInappIds()), completion: { (products, error) in
      var productsInfo: [String : [String: String]] = [:]
      if let products = products {
        let formatter = NumberFormatter()
        formatter.numberStyle = .currency
        for product in products {
          formatter.locale = product.priceLocale
          let formattedPrice = formatter.string(from: product.price) ?? ""
          let pd: [String: String] = ["price_string": formattedPrice]
          productsInfo[product.productId] = pd
        }
      }
      guard let jsonData = try? JSONSerialization.data(withJSONObject: productsInfo, options: []),
        let jsonString = String(data: jsonData, encoding: .utf8),
        let encodedString = jsonString.addingPercentEncoding(withAllowedCharacters: .urlHostAllowed) else {
          completion(nil)
          return
      }
      
      var result = MWMBookmarksManager.shared().getCatalogHeaders()
      result["X-Mapsme-Bundle-Tiers"] = encodedString
      completion(result)
    })
  }

  private func handlePendingTransactions(completion: @escaping (Bool) -> Void) {
    pendingTransactionsHandler.handlePendingTransactions { [weak self] (status) in
      switch status {
      case .none:
        fallthrough
      case .success:
        completion(true)
      case .error:
        MWMAlertViewController.activeAlert().presentInfoAlert(L("title_error_downloading_bookmarks"),
                                                              text: L("failed_purchase_support_message"))
        completion(false)
        self?.loadingIndicator.stopAnimating()
      case .needAuth:
        if let s = self, let navBar = s.navigationController?.navigationBar {
          s.signup(anchor: navBar, onComplete: {
            if $0 {
              s.handlePendingTransactions(completion: completion)
            } else {
              MWMAlertViewController.activeAlert().presentInfoAlert(L("title_error_downloading_bookmarks"),
                                                                    text: L("failed_purchase_support_message"))
              completion(false)
              s.loadingIndicator.stopAnimating()
            }
          })
        }
        break;
      }
    }
  }

  private func parseUrl(_ url: URL) -> CatalogCategoryInfo? {
    guard let urlComponents = URLComponents(url: url, resolvingAgainstBaseURL: false) else { return nil }
    guard let components = urlComponents.queryItems?.reduce(into: [:], { $0[$1.name] = $1.value })
      else { return nil }

    return CatalogCategoryInfo(components, type: SubscriptionGroupType(catalogURL: url))
  }

  func processDeeplink(_ url: URL) {
    self.deeplink = nil
    guard let categoryInfo = parseUrl(url) else {
      MWMAlertViewController.activeAlert().presentInfoAlert(L("title_error_downloading_bookmarks"),
                                                            text: L("subtitle_error_downloading_guide"))
      return
    }
    self.categoryInfo = categoryInfo

    download()
  }

  private func download() {
    guard let categoryInfo = self.categoryInfo else {
      assert(false)
      return
    }

    if MWMBookmarksManager.shared().isCategoryDownloading(categoryInfo.id) || MWMBookmarksManager.shared().hasCategoryDownloaded(categoryInfo.id) {
      MWMAlertViewController.activeAlert().presentDefaultAlert(withTitle: L("error_already_downloaded_guide"),
                                                               message: nil,
                                                               rightButtonTitle: L("ok"),
                                                               leftButtonTitle: nil,
                                                               rightButtonAction: nil)
      return
    }

    MWMBookmarksManager.shared().downloadItem(withId: categoryInfo.id, name: categoryInfo.name, progress: { [weak self] (progress) in
      self?.updateProgress()
    }) { [weak self] (categoryId, error) in
      if let error = error as NSError? {
        if error.code == kCategoryDownloadFailedCode {
          guard let statusCode = error.userInfo[kCategoryDownloadStatusKey] as? NSNumber else {
            assertionFailure()
            return
          }
          guard let status = MWMCategoryDownloadStatus(rawValue: statusCode.intValue) else {
            assertionFailure()
            return
          }
          switch (status) {
          case .needAuth:
            if let s = self, let navBar = s.navigationController?.navigationBar{
              s.signup(anchor: navBar) {
                if $0 { s.download() }
              }
            }
            break
          case .needPayment:
            self?.showPaymentScreen(categoryInfo)
            break
          case .notFound:
            self?.showServerError()
            break
          case .networkError:
            self?.showNetworkError()
            break
          case .diskError:
            self?.showDiskError()
            break
          }
        } else if error.code == kCategoryImportFailedCode {
          self?.showImportError()
        }
      } else {
        if MWMBookmarksManager.shared().getCatalogDownloadsCount() == 0 {
          Statistics.logEvent(kStatInappProductDelivered, withParameters: [kStatVendor: BOOKMARKS_VENDOR,
                                                                           kStatPurchase: categoryInfo.id],
                              with: .realtime)
          logToPushWoosh(categoryInfo)
          MapViewController.shared().showBookmarksLoadedAlert(categoryId)
        }
      }
      self?.updateProgress()
    }
  }

  private func showPaymentScreen(_ productInfo: CatalogCategoryInfo) {
    guard let productId = productInfo.productId else {
      MWMAlertViewController.activeAlert().presentInfoAlert(L("title_error_downloading_bookmarks"),
                                                            text: L("subtitle_error_downloading_guide"))
      return
    }

    let purchase = InAppPurchase.paidRoutePurchase(serverId: productInfo.id,
                                                   productId: productId)
    let testGroup = PromoCampaignManager.manager().paidRoutesSubscriptionCampaign.testGroupStatName
    let stats = InAppPurchase.paidRouteStatistics(serverId: productInfo.id,
                                                  productId: productId,
                                                  testGroup: testGroup)
    let paymentVC = PaidRouteViewController(name: productInfo.name,
                                            author: productInfo.author,
                                            imageUrl: URL(string: productInfo.imageUrl ?? ""),
                                            subscriptionType: productInfo.subscriptionType,
                                            purchase: purchase,
                                            statistics: stats)
    paymentVC.delegate = self
    paymentVC.modalTransitionStyle = .coverVertical
    self.navigationController?.present(paymentVC, animated: true)
  }

  private func showDiskError() {
    MWMAlertViewController.activeAlert().presentDownloaderNotEnoughSpaceAlert()
  }

  private func showNetworkError() {
    MWMAlertViewController.activeAlert().presentNoConnectionAlert();
  }

  private func showServerError() {
    MWMAlertViewController.activeAlert().presentDefaultAlert(withTitle: L("error_server_title"),
                                                             message: L("error_server_message"),
                                                             rightButtonTitle: L("try_again"),
                                                             leftButtonTitle: L("cancel")) {
                                                              self.download()
    }
  }

  private func showImportError() {
    MWMAlertViewController.activeAlert().presentInfoAlert(L("title_error_downloading_bookmarks"),
                                                          text: L("subtitle_error_downloading_guide"))
  }

  private func updateProgress() {
    let numberOfTasks = MWMBookmarksManager.shared().getCatalogDownloadsCount()
    numberOfTasksLabel.text = "\(numberOfTasks)"
    progressBgView.isHidden = numberOfTasks == 0
  }

  @objc private func onBack() {
    back()
  }

  @objc private func onFwd() {
    forward()
  }
}

private func logToPushWoosh(_ categoryInfo: CatalogCategoryInfo) {
  let pushManager = PushNotificationManager.push()
  
  if categoryInfo.productId == nil {
    pushManager!.setTags(["Bookmarks_Guides_free_title": categoryInfo.name]);
  } else {
    pushManager!.setTags(["Bookmarks_Guides_paid_tier": categoryInfo.productId!]);
    pushManager!.setTags(["Bookmarks_Guides_paid_title": categoryInfo.name]);
    pushManager!.setTags(["Bookmarks_Guides_paid_date": MWMPushNotifications.formattedTimestamp()]);
  }
}

extension CatalogWebViewController: PaidRouteViewControllerDelegate {
  func didCompleteSubscription(_ viewController: PaidRouteViewController) {
    dismiss(animated: true)
    download()
    self.webView.reloadFromOrigin()
  }

  func didCompletePurchase(_ viewController: PaidRouteViewController) {
    dismiss(animated: true)
    download()
  }

  func didCancelPurchase(_ viewController: PaidRouteViewController) {
    dismiss(animated: true)
  }
}
