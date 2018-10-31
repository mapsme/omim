import SafariServices

final class BookmarksSharingViewController: MWMTableViewController {
  typealias ViewModel = MWMAuthorizationViewModel
  
  var categoryId: MWMMarkGroupID?
  var categoryUrl: URL?
  
  @IBOutlet weak var uploadAndPublishCell: UploadActionCell!
  @IBOutlet weak var getDirectLinkCell: UploadActionCell!
  
  @IBOutlet private weak var licenseAgreementTextView: UITextView! {
    didSet {
      let htmlString = String(coreFormat: L("ugc_routes_user_agreement"), arguments: [ViewModel.termsOfUseLink()])
      let attributes: [NSAttributedStringKey : Any] = [NSAttributedStringKey.font: UIFont.regular14(),
                                                       NSAttributedStringKey.foregroundColor: UIColor.blackSecondaryText()]
      licenseAgreementTextView.attributedText = NSAttributedString.string(withHtml: htmlString,
                                                                    defaultAttributes: attributes)
      licenseAgreementTextView.delegate = self
    }
  }
  
  override func viewDidLoad() {
    super.viewDidLoad()
    
    title = L("sharing_options") //"Sharing options"
    self.configureActionCells()
    
    assert(categoryId != nil, "We can't share nothing")
  }
  
  func configureActionCells() {
    uploadAndPublishCell.config(titles: [ .normal : L("upload_and_publish"),
                                          .inProgress : L("upload_and_publish_progress_text"),
                                          .completed : L("upload_and_publish_success") ],
                                image: UIImage(named: "ic24PxGlobe")!,
                                delegate: self)
    getDirectLinkCell.config(titles: [ .normal : L("upload_and_get_direct_link"),
                                       .inProgress : L("direct_link_progress_text"),
                                       .completed : L("direct_link_success") ],
                             image: UIImage(named: "ic24PxLink")!, delegate: self)
  }
  
  override func tableView(_ tableView: UITableView, heightForRowAt indexPath: IndexPath) -> CGFloat {
    return UITableViewAutomaticDimension
  }
  
  override func tableView(_ tableView: UITableView,
                 titleForHeaderInSection section: Int) -> String? {
    return section == 0 ? L("public_access") : L("limited_access")
  }
  
  override func tableView(_ tableView: UITableView,
                          didSelectRowAt indexPath: IndexPath) {
    tableView.deselectRow(at: indexPath, animated: true)
    
    let cell = tableView.cellForRow(at: indexPath)
    if (cell == self.uploadAndPublishCell) {
      self.uploadAndPublish()
    } else if (cell == self.getDirectLinkCell) {
      self.uploadAndGetDirectLink()
    }
  }
  
  func uploadAndPublish() {
    self.performAfterValidation {
      //implementation
    }
  }
  
  func uploadAndGetDirectLink() {
    self.performAfterValidation { [unowned self] in
      MWMBookmarksManager.shared().uploadAndGetDirectLinkCategory(withId: self.categoryId!, progress: { (progress) in
        if (progress == .uploadStarted) {
          self.getDirectLinkCell.changeCellState(.inProgress)
        }
      }, completion: { (url, error) in
        if (error != nil) {
          self.getDirectLinkCell.changeCellState(.normal)
          //handle errors
        } else {
          self.getDirectLinkCell.changeCellState(.completed)
          self.categoryUrl = url
        }
      })
    }
  }
  
  func performAfterValidation(action: @escaping MWMVoidBlock) {
    MWMFrameworkHelper.checkConnectionAndPerformAction { [unowned self] in
      self.signup(anchor: self.view, onComplete: { success in
        if (success) {
          action()
        }
      })
    }
  }
}

extension BookmarksSharingViewController: UITextViewDelegate {
  func textView(_ textView: UITextView, shouldInteractWith URL: URL, in characterRange: NSRange) -> Bool {
    let safari = SFSafariViewController(url: URL)
    self.present(safari, animated: true, completion: nil)
    return false;
  }
}

extension BookmarksSharingViewController: UploadActionCellDelegate {
  func cellDidPressShareButton(_ cell: UploadActionCell) {
    let message = L("share_bookmarks_email_body")
    let shareController = MWMActivityViewController.share(for: categoryUrl, message: message)
    shareController?.present(inParentViewController: self, anchorView: nil)
  }
}
