@objc(MWMUGCAddReviewCell)
final class UGCAddReviewCell: MWMTableViewCell {
  @IBOutlet private weak var titleLabel: UILabel!

  @IBOutlet private weak var horribleButton: UIButton!
  @IBOutlet private weak var badButton: UIButton!
  @IBOutlet private weak var normalButton: UIButton!
  @IBOutlet private weak var goodButton: UIButton!
  @IBOutlet private weak var excellentButton: UIButton!
  @IBOutlet private weak var horribleLabel: UILabel! 
  @IBOutlet private weak var badLabel: UILabel!
  @IBOutlet private weak var normalLabel: UILabel!
  @IBOutlet private weak var goodLabel: UILabel!
  @IBOutlet private weak var excellentLabel: UILabel!

  @objc var onRateTap: ((MWMRatingSummaryViewValueType) -> Void)!

  @IBAction private func rate(_ button: UIButton) {
    switch button {
    case horribleButton: onRateTap(.horrible)
    case badButton: onRateTap(.bad)
    case normalButton: onRateTap(.normal)
    case goodButton: onRateTap(.good)
    case excellentButton: onRateTap(.excellent)
    default: assert(false)
    }
  }
}
