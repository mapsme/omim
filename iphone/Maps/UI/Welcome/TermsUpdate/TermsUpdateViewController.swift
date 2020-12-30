class TermsUpdateViewController: MWMViewController {
  @IBOutlet var desciptionTextView: UITextView!
  private let transitioning = FadeTransitioning<IPadModalPresentationController>()

  let privacyPolicyLink = User.privacyPolicyLink()
  let termsOfUseLink = User.termsOfUseLink()

  override init(nibName nibNameOrNil: String?, bundle nibBundleOrNil: Bundle?) {
    super.init(nibName: nibNameOrNil, bundle: nibBundleOrNil)
    transitioningDelegate = transitioning
    modalPresentationStyle = .custom
  }

  required init?(coder aDecoder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }

  override func viewDidLoad() {
    super.viewDidLoad()
    let htmlString = String(coreFormat: L("licence_agreement_popup_message"), arguments: [termsOfUseLink, privacyPolicyLink, termsOfUseLink, privacyPolicyLink])
    desciptionTextView.attributedText = NSAttributedString.string(withHtml: htmlString,
                                                                  defaultAttributes: [:])

    preferredContentSize = UIScreen.main.bounds.size
  }

  @IBAction func onApplyButtonPress(_ sender: Any) {
    WelcomeStorage.shouldShowTerms102 = false
    WelcomeStorage.privacyPolicyLink = privacyPolicyLink
    WelcomeStorage.termsOfUseLink = termsOfUseLink
    WelcomeStorage.acceptTime = Date()
    dismiss(animated: false, completion: nil)
  }

  @IBAction func onSkipButtonPress(_ sender: Any) {
    dismiss(animated: true, completion: nil)
  }
}
