protocol CatalogCategoryCellDelegate : AnyObject {
  func cell(_ cell: CatalogCategoryCell, didCheck visible: Bool)
  func cell(_ cell: CatalogCategoryCell, didPress moreButton: UIButton)
}

final class CatalogCategoryCell: MWMTableViewCell {

  weak var delegate: CatalogCategoryCellDelegate?

  @IBOutlet weak var visibleCheckmark: Checkmark! {
    didSet {
      visibleCheckmark.offTintColor = .blackHintText()
      visibleCheckmark.onTintColor = .linkBlue()
    }
  }
  @IBOutlet weak var titleLabel: UILabel! {
    didSet {
      titleLabel.font = .regular16()
      titleLabel.textColor = .blackPrimaryText()
    }
  }
  @IBOutlet weak var subtitleLabel: UILabel! {
    didSet {
      subtitleLabel.font = .regular14()
      subtitleLabel.textColor = .blackSecondaryText()
    }
  }
  @IBOutlet weak var moreButton: UIButton!

  @IBAction func onVisibleChanged(_ sender: Checkmark) {
    delegate?.cell(self, didCheck: sender.isChecked)
  }

  @IBAction func onMoreButton(_ sender: UIButton) {
    delegate?.cell(self, didPress: sender)
  }

  func update(with category: MWMCatalogCategory, delegate: CatalogCategoryCellDelegate?) {
    titleLabel.text = category.title
    let placesString = String(format: L("bookmarks_places"), category.bookmarksCount)
    let authorString = String(coreFormat: L("author_name_by_prefix"), arguments: [category.author])
    subtitleLabel.text = "\(placesString) • \(authorString)"
    visibleCheckmark.isChecked = category.visible
    self.delegate = delegate
  }
}
