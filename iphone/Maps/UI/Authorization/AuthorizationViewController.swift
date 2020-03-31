import FBSDKCoreKit
import FBSDKLoginKit
import GoogleSignIn
import SafariServices

@objc enum AuthorizationError: Int {
  case cancelled
  case passportError
}

@objc(MWMAuthorizationViewController)
final class AuthorizationViewController: MWMViewController {

  private let transitioningManager: AuthorizationTransitioningManager

  lazy var chromeView: UIView = {
    let view = UIView()
    view.styleName = "BlackStatusBarBackground"
    return view
  }()

  weak var containerView: UIView! {
    didSet {
      containerView.insertSubview(chromeView, at: 0)
    }
  }

  @IBOutlet private weak var contentView: UIView!
  @IBOutlet private weak var titleLabel: UILabel!
  @IBOutlet weak var separator: UIView!
  @IBOutlet private weak var textLabel: UILabel!

  @IBOutlet private weak var googleButton: UIButton! {
    didSet {
      googleButton.setTitle("Google", for: .normal)
      googleButton.isEnabled = false
    }
  }

  @IBAction func googleSignIn() {
    let gid = GIDSignIn.sharedInstance()!
    if var scopes = gid.scopes {
      scopes.append("https://www.googleapis.com/auth/plus.login")
      gid.scopes = scopes
    }
    gid.delegate = self
    gid.uiDelegate = self
    gid.signIn()
  }

  @IBOutlet private weak var facebookButton: UIButton! {
    didSet {
      facebookButton.isEnabled = false
    }
  }
  
  @IBAction func facebookSignIn() {
    let fbLoginManager = LoginManager()
    fbLoginManager.logIn(permissions: ["public_profile", "email"], from: self) { [weak self] (result, error) in
      if let error = error {
        self?.process(error: error, type: .facebook)
      } else if let token = result?.token {
        self?.process(token: token.tokenString, type: .facebook)
      }
    }
  }

  @IBAction private func phoneSignIn() {
    let authVC = PhoneNumberAuthorizationViewController(success: { [unowned self] token in
      self.dismiss(animated: true)
      self.process(token: token!, type: .phone)
    }, failure: { [unowned self] in
      self.dismiss(animated: true)
      self.process(error: NSError(domain: kMapsmeErrorDomain, code: 0), type: .phone)
      self.errorHandler?(.cancelled)
    })
    let navVC = MWMNavigationController(rootViewController: authVC)
    self.present(navVC, animated: true)
  }
  
  @IBOutlet private weak var phoneSignInButton: UIButton! {
    didSet {
      phoneSignInButton.isEnabled = false
    }
  }

  @IBOutlet private weak var privacyPolicyCheck: Checkmark!
  @IBOutlet private weak var termsOfUseCheck: Checkmark!
  @IBOutlet private weak var latestNewsCheck: Checkmark!
  
  @IBAction func onCheck(_ sender: Checkmark) {
    let allButtonsChecked = privacyPolicyCheck.isChecked &&
      termsOfUseCheck.isChecked;
    
    googleButton.isEnabled = allButtonsChecked;
    facebookButton.isEnabled = allButtonsChecked;
    phoneSignInButton.isEnabled = allButtonsChecked;
  }
  
  @IBOutlet private weak var privacyPolicyTextView: UITextView! {
    didSet {
      let htmlString = String(coreFormat: L("sign_agree_pp_gdpr"), arguments: [User.privacyPolicyLink()])
      privacyPolicyTextView.attributedText = NSAttributedString.string(withHtml: htmlString,
                                                                       defaultAttributes: [:])
      privacyPolicyTextView.delegate = self
    }
  }
  
  @IBOutlet private weak var termsOfUseTextView: UITextView! {
    didSet {
      let htmlString = String(coreFormat: L("sign_agree_tof_gdpr"), arguments: [User.termsOfUseLink()])
      termsOfUseTextView.attributedText = NSAttributedString.string(withHtml: htmlString,
                                                                    defaultAttributes: [:])
      termsOfUseTextView.delegate = self
    }
  }
  
  @IBOutlet private weak var latestNewsTextView: UITextView! {
    didSet {
      let text = L("sign_agree_news_gdpr")
      latestNewsTextView.attributedText = NSAttributedString(string: text, attributes: [:])
    }
  }
  
  @IBOutlet private weak var topToContentConstraint: NSLayoutConstraint!

  typealias SuccessHandler = (SocialTokenType) -> Void
  typealias ErrorHandler = (AuthorizationError) -> Void
  typealias CompletionHandler = (AuthorizationViewController) -> Void

  private let sourceComponent: AuthorizationSource
  private let successHandler: SuccessHandler?
  private let errorHandler: ErrorHandler?
  private let completionHandler: CompletionHandler?

  @objc
  init(barButtonItem: UIBarButtonItem?, sourceComponent: AuthorizationSource, successHandler: SuccessHandler? = nil, errorHandler: ErrorHandler? = nil, completionHandler: CompletionHandler? = nil) {
    self.sourceComponent = sourceComponent
    self.successHandler = successHandler
    self.errorHandler = errorHandler
    self.completionHandler = completionHandler
    transitioningManager = AuthorizationTransitioningManager(barButtonItem: barButtonItem)
    super.init(nibName: toString(type(of: self)), bundle: nil)
    transitioningDelegate = transitioningManager
    modalPresentationStyle = .custom
  }

  @objc
  init(popoverSourceView: UIView? = nil, sourceComponent: AuthorizationSource, permittedArrowDirections: UIPopoverArrowDirection = .unknown, successHandler: SuccessHandler? = nil, errorHandler: ErrorHandler? = nil, completionHandler: CompletionHandler? = nil) {
    self.sourceComponent = sourceComponent
    self.successHandler = successHandler
    self.errorHandler = errorHandler
    self.completionHandler = completionHandler
    transitioningManager = AuthorizationTransitioningManager(popoverSourceView: popoverSourceView, permittedArrowDirections: permittedArrowDirections)
    super.init(nibName: toString(type(of: self)), bundle: nil)
    transitioningDelegate = transitioningManager
    modalPresentationStyle = .custom
  }

  required init?(coder _: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }
  
  override func viewDidLoad() {
    super.viewDidLoad()
    iPadSpecific {
      topToContentConstraint.isActive = false
    }
  }

  override func viewDidAppear(_ animated: Bool) {
    super.viewDidAppear(animated)
  }

  override func viewDidLayoutSubviews() {
    super.viewDidLayoutSubviews()
    iPadSpecific {
      preferredContentSize = contentView.systemLayoutSizeFitting(preferredContentSize, withHorizontalFittingPriority: .fittingSizeLevel, verticalFittingPriority: .fittingSizeLevel)
    }
  }

  @IBAction func onCancel() {
    Statistics.logEvent(kStatUGCReviewAuthDeclined)
    errorHandler?(.cancelled)
    onClose()
  }

  private func onClose() {
    dismiss(animated: true)
    completionHandler?(self)
  }
  
  private func getProviderStatStr(type: SocialTokenType) -> String {
    switch type {
    case .facebook: return kStatFacebook
    case .google: return kStatGoogle
    case .phone: return kStatPhone
    @unknown default:
      fatalError()
    }
  }

  private func process(error: Error, type: SocialTokenType) {
    Statistics.logEvent(kStatUGCReviewAuthError, withParameters: [
      kStatProvider: getProviderStatStr(type: type),
      kStatError: error.localizedDescription,
    ])
    textLabel.text = L("profile_authorization_error")
    Crashlytics.sharedInstance().recordError(error)
  }

  private func process(token: String, type: SocialTokenType) {
    Statistics.logEvent(kStatUGCReviewAuthExternalRequestSuccess,
                        withParameters: [kStatProvider: getProviderStatStr(type: type)])
    User.authenticate(withToken: token,
                      type: type,
                      privacyAccepted: privacyPolicyCheck.isChecked,
                      termsAccepted: termsOfUseCheck.isChecked,
                      promoAccepted: latestNewsCheck.isChecked,
                      source: sourceComponent) { success in
                        self.logStats(type: type, success: success)
                        if success {
                          self.successHandler?(type)
                        } else {
                          self.errorHandler?(.passportError)
                        }
    }
    onClose()
  }

  private func logStats(type: SocialTokenType, success: Bool) {
    let provider: String
    switch type {
    case .google:
      provider = kStatGoogle
    case .facebook:
      provider = kStatFacebook
    case .phone:
      provider = kStatPhone
    @unknown default:
      fatalError()
    }

    let event: String?
    switch self.sourceComponent {
    case .UGC:
      event = success ? kStatUGCReviewAuthRequestSuccess : kStatUGCReviewAuthError
    case .bookmarks:
      event = success ? kStatBookmarksAuthRequestSuccess : kStatBookmarksAuthRequestError
    @unknown default:
      fatalError()
    }

    if success {
      Statistics.logEvent(event, withParameters: [kStatProvider : provider])
    } else {
      Statistics.logEvent(event, withParameters: [kStatProvider : provider, kStatError : ""])
    }
  }
}

extension AuthorizationViewController: GIDSignInUIDelegate {
}

extension AuthorizationViewController: GIDSignInDelegate {
  func sign(_: GIDSignIn!, didSignInFor user: GIDGoogleUser!, withError error: Error!) {
    if let error = error {
      process(error: error, type: .google)
    } else {
      process(token: user.authentication.idToken, type: .google)
    }
  }
}

extension AuthorizationViewController: UITextViewDelegate {
  func textView(_ textView: UITextView, shouldInteractWith URL: URL, in characterRange: NSRange) -> Bool {
    let safari = SFSafariViewController(url: URL)
    self.present(safari, animated: true, completion: nil)
    return false;
  }
}
