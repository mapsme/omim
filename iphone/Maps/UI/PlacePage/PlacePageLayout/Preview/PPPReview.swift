@objc(MWMPPPReview)
final class PPPReview: MWMTableViewCell {
  @IBOutlet private weak var ratingSummaryView: RatingSummaryView!
  @IBOutlet private weak var reviewsLabel: UILabel!
  @IBOutlet private weak var pricingLabel: UILabel!
  @IBOutlet private weak var addReviewButton: UIButton!

  @IBOutlet private weak var discountView: UIView!
  @IBOutlet private weak var discountLabel: UILabel!
  @IBOutlet private weak var priceConstraint: NSLayoutConstraint!

  typealias OnAddReview = () -> ()
  private var onAddReview: OnAddReview?

  @objc func config(rating: UGCRatingValueType,
                    canAddReview: Bool,
                    isReviewedByUser: Bool,
                    reviewsCount: UInt,
                    ratingsCount: UInt,
                    price: String,
                    discount: Int,
                    smartDeal: Bool,
                    onAddReview: OnAddReview?) {
    self.onAddReview = onAddReview
    pricingLabel.text = price
    if discount > 0 {
      priceConstraint.priority = .defaultLow
      discountView.isHidden = false
      discountLabel.text = "-\(discount)%"
    } else if smartDeal {
      priceConstraint.priority = .defaultLow
      discountView.isHidden = false
      discountLabel.text = "%"
    } else {
      priceConstraint.priority = .defaultHigh
      discountView.isHidden = true
    }
    
    ratingSummaryView.value = rating.value
    ratingSummaryView.type = rating.type
    ratingSummaryView.backgroundOpacity = 0.05
    if canAddReview && !isReviewedByUser {
      addReviewButton.isHidden = false
      addReviewButton.layer.cornerRadius = addReviewButton.height / 2
    } else {
      addReviewButton.isHidden = true
    }
    pricingLabel.isHidden = true
    reviewsLabel.isHidden = false
    if rating.type == .noValue {
      if isReviewedByUser {
        ratingSummaryView.setStyleAndApply("RatingSummaryView12User")
        reviewsLabel.text = L("placepage_reviewed")
        pricingLabel.isHidden = false
      } else {
        ratingSummaryView.setStyleAndApply("RatingSummaryView12")
        reviewsLabel.text = reviewsCount == 0 ? L("placepage_no_reviews") : ""
      }
    } else {
      ratingSummaryView.setStyleAndApply("RatingSummaryView12")

      reviewsLabel.text = ratingsCount > 0
        ? String(format:L("placepage_summary_rating_description"), ratingsCount) : ""
      pricingLabel.isHidden = false
    }
  }

  @IBAction private func addReview() {
    onAddReview?()
  }

  override func layoutSubviews() {
    super.layoutSubviews()
    let inset = width / 2
    separatorInset = UIEdgeInsets.init(top: 0, left: inset, bottom: 0, right: inset)
  }
}
