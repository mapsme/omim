class SearchStyleSheet: IStyleSheet {
  static func register(theme: Theme, colors: IColors, fonts: IFonts) {
    theme.add(styleName: "SearchInstallButton") { (s) -> (Void) in
      s.cornerRadius = 10
      s.clip = true
      s.font = fonts.medium12
      s.fontColor = colors.blackSecondaryText
      s.backgroundColor = colors.searchPromoBackground
    }

    theme.add(styleName: "SearchBanner") { (s) -> (Void) in
      s.backgroundColor = colors.searchPromoBackground
    }

    theme.add(styleName: "SearchClosedBackground") { (s) -> (Void) in
      s.cornerRadius = 4
      s.backgroundColor = colors.blackHintText
    }

    theme.add(styleName: "SearchPopularView") { (s) -> (Void) in
      s.cornerRadius = 10
      s.backgroundColor = colors.linkBlueHighlighted
    }

    theme.add(styleName: "SearchSideAvaliableMarker") { (s) -> (Void) in
      s.backgroundColor = colors.ratingGreen
    }

    theme.add(styleName: "SearchBarView") { (s) -> (Void) in
      s.backgroundColor = colors.primary
      s.shadowRadius = 2
      s.shadowColor = UIColor(0,0,0, alpha26)
      s.shadowOpacity = 1
      s.shadowOffset = CGSize.zero
    }
    
    theme.add(styleName: "SearchFilterButtonActive") { (s) -> (Void) in
      s.backgroundColor = colors.linkBlue
      s.backgroundColorHighlighted = colors.linkBlueHighlighted
      s.fontColor = colors.white
      s.cornerRadius = 4
      s.font = fonts.regular17
    }

    theme.add(styleName: "SearchFilterButtonInActive") { (s) -> (Void) in
      s.backgroundColor = colors.clear
      s.backgroundColorHighlighted = colors.clear
      s.fontColor = colors.linkBlue
      s.cornerRadius = 4
      s.font = fonts.regular17
    }

    theme.add(styleName: "SearchCancelButtonActive") { (s) -> (Void) in
      s.tintColor = colors.white
      s.image = "ic_clear_filters"
      s.coloring = MWMButtonColoring.white
    }

    theme.add(styleName: "SearchCancelButtonInActive") { (s) -> (Void) in
      s.tintColor = colors.linkBlueHighlighted
      s.image = "ic_filter"
      s.coloring = MWMButtonColoring.blue
    }

    theme.add(styleName: "SearchChangeModeView") { (s) -> (Void) in
      s.backgroundColor = colors.pressBackground
      s.shadowRadius = 2
      s.shadowColor = UIColor(0, 0, 0, 0.24);
      s.shadowOffset = CGSize.zero
      s.shadowOpacity = 1
    }

    theme.add(styleName: "SearchSearchTextField") { (s) -> (Void) in
      s.fontColor = colors.blackSecondaryText
      s.backgroundColor = colors.white
      s.tintColor = colors.blackSecondaryText
    }

    theme.add(styleName: "SearchSearchTextFieldIcon") { (s) -> (Void) in
      s.tintColor = colors.blackSecondaryText
      s.coloring = MWMButtonColoring.black
      s.color = colors.blackSecondaryText
    }

    theme.add(styleName: "FilterRatingButton") { (s) -> (Void) in
      s.cornerRadius = 4
      s.borderWidth = 1
      s.borderColor = colors.blackDividers
    }

    theme.add(styleName: "SearchFilterTypeCell") { (s) -> (Void) in
      s.cornerRadius = 16
      s.borderColor = colors.blackDividers
      s.borderWidth = 1
    }

    theme.add(styleName: "FilterCheckButton") { (s) -> (Void) in
      s.fontColor = colors.blackPrimaryText
      s.fontColorDisabled = colors.blackDividers
      s.backgroundColor = colors.white
      s.font = fonts.regular14
      s.cornerRadius = 4
      s.borderWidth = 1
      s.borderColor = colors.blackDividers
      s.textAlignment = .natural
    }

    theme.add(styleName: "SearchCellAds", from: "TableCell") { (s) -> (Void) in
      s.backgroundColor = colors.searchPromoBackground
    }

    theme.add(styleName: "SearchCellAvaliable", from: "TableCell") { (s) -> (Void) in
      s.backgroundColor = colors.transparentGreen
    }
  }
}
