@objc(MWMUGCSpecificReviewDelegate)
protocol UGCSpecificReviewDelegate: NSObjectProtocol {
  func changeReviewRate(_ rate: Int, atIndexPath: NSIndexPath)
}

@objc(MWMUGCSpecificReviewCell)
final class UGCSpecificReviewCell: MWMTableViewCell {
  @IBOutlet private weak var specification: UILabel!
  @IBOutlet private var stars: [UIButton]!
  private var indexPath: NSIndexPath = NSIndexPath()
  private var delegate: UGCSpecificReviewDelegate?

  func configWith(specification: String, rate: Int, atIndexPath: NSIndexPath, delegate: UGCSpecificReviewDelegate?) {
    self.specification.text = specification
    self.delegate = delegate
    indexPath = atIndexPath
    stars.forEach { $0.isSelected = $0.tag <= rate }
  }

  @IBAction private func tap(on: UIButton) {
    stars.forEach { $0.isSelected = $0.tag <= on.tag }
    delegate?.changeReviewRate(on.tag, atIndexPath: indexPath)
  }

  //TODO: Make highlighting and dragging.

  @IBAction private func highlight(on: UIButton) { }

  @IBAction private func touchingCanceled(on: UIButton) { }

  @IBAction private func drag(inside: UIButton) { }
}
