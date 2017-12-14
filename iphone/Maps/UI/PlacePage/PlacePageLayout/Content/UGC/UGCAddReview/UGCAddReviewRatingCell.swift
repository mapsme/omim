final class UGCAddReviewRatingCell: MWMTableViewCell {
  @IBOutlet weak var titleLabel: UILabel! {
    didSet {
      titleLabel.font = UIFont.regular16()
      titleLabel.textColor = UIColor.blackPrimaryText()
    }
  }

  @IBOutlet weak var ratingView: RatingView! {
    didSet {
      ratingView.filledColor = UIColor.ratingYellow()
      ratingView.emptyColor = UIColor.blackDividers()
      ratingView.borderWidth = 0
      ratingView.delegate = self
    }
  }

  private weak var delegate: UGCAddReviewRatingCellDelegate?

  private var model: UGCRatingStars! {
    didSet {
      titleLabel.text = L(model.title)
      ratingView.value = model.value
      ratingView.starsCount = 5
    }
  }

  func config(model: UGCRatingStars, delegate: UGCAddReviewRatingCellDelegate) {
    self.model = model
    self.delegate = delegate
  }
}

extension UGCAddReviewRatingCell: RatingViewDelegate {
  func didTouchRatingView(_ view: RatingView) {
    model.value = view.value
  }

  func didFinishTouchingRatingView(_: RatingView) {
    delegate?.onFinishedTouchingRatingView()
  }
}
