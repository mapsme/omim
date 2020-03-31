class PlacePageStyleSheet: IStyleSheet {
  static func register(theme: Theme, colors: IColors, fonts: IFonts) {
    theme.add(styleName: "PPReviewDiscountView") { (s) -> (Void) in
      s.backgroundColor = colors.linkBlue
      s.round = true
    }

    theme.add(styleName: "PPTitlePopularView") { (s) -> (Void) in
      s.backgroundColor = colors.linkBlueHighlighted
      s.cornerRadius = 10
    }

    theme.add(styleName: "ElevationProfileDescriptionCell") { (s) -> (Void) in
      s.backgroundColor = colors.blackOpaque
      s.cornerRadius = 6
    }

    theme.add(styleName: "ElevationProfileExtendedDifficulty") { (s) -> (Void) in
      s.backgroundColor = colors.blackSecondaryText
      s.fontColor = colors.white
      s.font = fonts.medium14
      s.textContainerInset = UIEdgeInsets(top: 4, left: 6, bottom: 4, right: 6)
    }

    theme.add(styleName: "RouteBasePreview") { (s) -> (Void) in
      s.borderColor = colors.blackDividers
      s.borderWidth = 1
      s.backgroundColor = colors.white
    }

    theme.add(styleName: "RoutePreview") { (s) -> (Void) in
      s.shadowRadius = 2
      s.shadowColor = colors.blackDividers
      s.shadowOpacity = 1
      s.shadowOffset = CGSize(width: 3, height: 0)
      s.backgroundColor = colors.pressBackground
    }

    theme.add(styleName: "RatingSummaryView24") { (s) -> (Void) in
      s.font = fonts.bold16
      s.fontColorHighlighted = colors.ratingYellow //filled color
      s.fontColorDisabled = colors.blackDividers //empty color
      s.colors = [
        colors.blackSecondaryText, //noValue
        colors.ratingRed, //horrible
        colors.ratingOrange, //bad
        colors.ratingYellow, //normal
        colors.ratingLightGreen, //good
        colors.ratingGreen //exellent
      ]
      s.images = [
        "ic_24px_rating_normal", //noValue
        "ic_24px_rating_horrible", //horrible
        "ic_24px_rating_bad", //bad
        "ic_24px_rating_normal", //normal
        "ic_24px_rating_good", //good
        "ic_24px_rating_excellent" //exellent
      ]
    }

    theme.add(styleName: "RatingSummaryView12", from: "RatingSummaryView24") { (s) -> (Void) in
      s.font = fonts.bold12
      s.images = [
        "ic_12px_rating_normal",
        "ic_12px_rating_horrible",
        "ic_12px_rating_bad",
        "ic_12px_rating_normal",
        "ic_12px_rating_good",
        "ic_12px_rating_excellent"
      ]
    }

    theme.add(styleName: "RatingSummaryView12User", from: "RatingSummaryView12") { (s) -> (Void) in
      s.colors?[0] = colors.linkBlue
      s.images?[0] = "ic_12px_radio_on"
    }
  }
}
