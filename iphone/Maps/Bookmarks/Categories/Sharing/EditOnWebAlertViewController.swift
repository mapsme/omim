class EditOnWebAlertViewController: UIViewController {
  private let transitioning = FadeTransitioning<AlertPresentationController>()
  private var alertTitle = ""
  private var alertMessage = ""

  var onAcceptBlock: MWMVoidBlock?
  var onCancelBlock: MWMVoidBlock?

  @IBOutlet weak var titleLabel: UILabel!
  @IBOutlet weak var messageLabel: UILabel!
  @IBOutlet weak var cancelButton: UIButton!
  @IBOutlet weak var acceptButton: UIButton! {
    didSet {
      acceptButton.setTitle(L("edit_on_web").uppercased(), for: .normal)
    }
  }

  convenience init(with title: String, message: String) {
    self.init()
    alertTitle = title
    alertMessage = message
  }

  override func viewDidLoad() {
    super.viewDidLoad()

    titleLabel.text = alertTitle
    messageLabel.text = alertMessage
  }

  override var transitioningDelegate: UIViewControllerTransitioningDelegate? {
    get { return transitioning }
    set { }
  }

  override var modalPresentationStyle: UIModalPresentationStyle {
    get { return .custom }
    set { }
  }

  @IBAction func onAccept(_ sender: UIButton) {
    onAcceptBlock?()
  }

  @IBAction func onCancel(_ sender: UIButton) {
    onCancelBlock?()
  }
}
