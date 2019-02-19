protocol GuideDescriptionViewControllerDelegate {
  func viewController(_ viewController: GuideDescriptionViewController, didFinishEditing text: String)
}

fileprivate let kLengthLimit = 500

class GuideDescriptionViewController: MWMTableViewController {
  @IBOutlet weak var nextBarButton: UIBarButtonItem!
  @IBOutlet weak var descriptionTextView: MWMTextView!

  var delegate: GuideDescriptionViewControllerDelegate?
  var guideDescription: String? {
    didSet {
      if let str = guideDescription, str.count > kLengthLimit {
        guideDescription = String(str.prefix(kLengthLimit))
      }
    }
  }

  override func viewDidLoad() {
    super.viewDidLoad()
    title = L("description_guide")
    descriptionTextView.placeholder = L("description_placeholder")
    descriptionTextView.text = guideDescription
    descriptionTextView.becomeFirstResponder()
    nextBarButton.isEnabled = descriptionTextView.text.count > 0
  }

  override func tableView(_ tableView: UITableView, titleForHeaderInSection section: Int) -> String? {
    return L("description_title")
  }

  override func tableView(_ tableView: UITableView, titleForFooterInSection section: Int) -> String? {
    return L("description_comment1")
  }

  @IBAction func onNext(_ sender: UIBarButtonItem) {
    delegate?.viewController(self, didFinishEditing: descriptionTextView.text)
  }
}

extension GuideDescriptionViewController: UITextViewDelegate {
  func textViewDidChange(_ textView: UITextView) {
    nextBarButton.isEnabled = textView.text.count > 0
  }

  func textView(_ textView: UITextView,
                shouldChangeTextIn range: NSRange,
                replacementText text: String) -> Bool {
    let textViewText = textView.text as NSString
    let resultText = textViewText.replacingCharacters(in: range, with: text)
    return resultText.count <= kLengthLimit
  }
}
