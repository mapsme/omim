@objc protocol CategorySettingsViewControllerDelegate: AnyObject {
  func categorySettingsController(_ viewController: CategorySettingsViewController,
                                  didEndEditing categoryId: MWMMarkGroupID)
  func categorySettingsController(_ viewController: CategorySettingsViewController,
                                  didDelete categoryId: MWMMarkGroupID)
}

final class CategorySettingsViewController: MWMTableViewController {
  private enum Sections: Int {
    case info
    case description
    case delete
    case count
  }

  private enum InfoSectionRows: Int {
    case title
    case sharingOptions
  }

  private let bookmarkGroup: BookmarkGroup
  private var noteCell: MWMNoteCell?
  private var changesMade = false
  private var newName: String?
  private var newAnnotation: String?

  @objc weak var delegate: CategorySettingsViewControllerDelegate?
  
  @objc init(bookmarkGroup: BookmarkGroup) {
    self.bookmarkGroup = bookmarkGroup
    super.init(style: .grouped)
  }

  required init?(coder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }

  override func viewDidLoad() {
    super.viewDidLoad()

    title = L("list_settings")
    navigationItem.rightBarButtonItem = UIBarButtonItem(barButtonSystemItem: .save,
                                                        target: self,
                                                        action: #selector(onSave))

    tableView.registerNib(cell: BookmarkTitleCell.self)
    tableView.registerNib(cell: MWMButtonCell.self)
    tableView.registerNib(cell: MWMNoteCell.self)
  }
  
  override func viewDidDisappear(_ animated: Bool) {
    super.viewDidDisappear(animated)
    if isMovingFromParent && !changesMade {
      Statistics.logEvent(kStatBookmarkSettingsCancel)
    }
  }

  override func numberOfSections(in tableView: UITableView) -> Int {
    Sections.count.rawValue
  }
  
  override func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
    switch Sections(rawValue: section) {
    case .info:
      return bookmarkGroup.isEmpty ? 1 : 2
    case .description, .delete:
      return 1
    default:
      fatalError()
    }
  }

  override func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
    switch Sections(rawValue: indexPath.section) {
    case .info:
      switch InfoSectionRows(rawValue: indexPath.row) {
      case .title:
        let cell = tableView.dequeueReusableCell(cell: BookmarkTitleCell.self, indexPath: indexPath)
        cell.configure(name: bookmarkGroup.title, delegate: self)
        return cell
      case .sharingOptions:
        let cell = tableView.dequeueDefaultCell(for: indexPath)
        cell.accessoryType = .disclosureIndicator
        cell.textLabel?.text = L("sharing_options")
        return cell
      default:
        fatalError()
      }
    case .description:
      if let noteCell = noteCell {
        return noteCell
      } else {
        let cell = tableView.dequeueReusableCell(cell: MWMNoteCell.self, indexPath: indexPath)
        cell.config(with: self, noteText: bookmarkGroup.detailedAnnotation,
                    placeholder: L("ugc_route_edit_description_hint"))
        noteCell = cell
        return cell
      }
    case .delete:
      let cell = tableView.dequeueReusableCell(cell: MWMButtonCell.self, indexPath: indexPath)
      cell.configure(with: self, title: L("delete_list"))
      return cell
    default:
      fatalError()
    }
  }

  override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
    switch InfoSectionRows(rawValue: indexPath.row) {
    case .sharingOptions:
      let sharingViewController = UIStoryboard.instance(.sharing).instantiateInitialViewController()
        as! BookmarksSharingViewController
      sharingViewController.category = bookmarkGroup
      navigationController?.pushViewController(sharingViewController, animated: true)
      Statistics.logEvent(kStatBookmarkSettingsClick, withParameters: [kStatOption : kStatSharingOptions])
      break
    default:
      break
    }
  }

  @objc func onSave() {
    view.endEditing(true)
    if let newName = newName, !newName.isEmpty {
      bookmarkGroup.title = newName
      changesMade = true
    }

    if let newAnnotation = newAnnotation, !newAnnotation.isEmpty {
      bookmarkGroup.detailedAnnotation = newAnnotation
      changesMade = true
    }

    delegate?.categorySettingsController(self, didEndEditing: bookmarkGroup.categoryId)
    Statistics.logEvent(kStatBookmarkSettingsConfirm)
  }
}

extension CategorySettingsViewController: BookmarkTitleCellDelegate {
  func didFinishEditingTitle(_ title: String) {
    newName = title
  }
}

extension CategorySettingsViewController: MWMNoteCellDelegate {
  func cell(_ cell: MWMNoteCell, didChangeSizeAndText text: String) {
    UIView.setAnimationsEnabled(false)
    tableView.refresh()
    UIView.setAnimationsEnabled(true)
    tableView.scrollToRow(at: IndexPath(item: 0, section: Sections.description.rawValue),
                          at: .bottom,
                          animated: true)
  }

  func cell(_ cell: MWMNoteCell, didFinishEditingWithText text: String) {
    newAnnotation = text
  }
}

extension CategorySettingsViewController: MWMButtonCellDelegate {
  func cellDidPressButton(_ cell: UITableViewCell) {
    BookmarksManager.shared().deleteCategory(bookmarkGroup.categoryId)
    delegate?.categorySettingsController(self, didDelete: bookmarkGroup.categoryId)
    Statistics.logEvent(kStatBookmarkSettingsClick,
                        withParameters: [kStatOption : kStatDelete])
  }
}
