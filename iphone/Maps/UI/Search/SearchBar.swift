final class SearchBar: SolidTouchView {
  @IBOutlet private var cancelButton: UIButton!

  override var visibleAreaAffectDirections: MWMAvailableAreaAffectDirections { return alternative(iPhone: .top, iPad: .left) }

  override var placePageAreaAffectDirections: MWMAvailableAreaAffectDirections { return alternative(iPhone: [], iPad: .left) }

  override var widgetsAreaAffectDirections: MWMAvailableAreaAffectDirections { return alternative(iPhone: [], iPad: .left) }

  override var trafficButtonAreaAffectDirections: MWMAvailableAreaAffectDirections { return alternative(iPhone: .top, iPad: .left) }

  override var menuAreaAffectDirections: MWMAvailableAreaAffectDirections { return alternative(iPhone: [], iPad: .left) }

  override func mwm_refreshUI() {
    super.mwm_refreshUI()
    cancelButton.setTitleColor(UIColor.whitePrimaryText(), for: .normal)
  }
}
