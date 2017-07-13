import UIKit

class SearchBar: SolidTouchView {
  override var visibleAreaAffectDirections: MWMAvailableAreaAffectDirections { return alternative(iPhone: .top, iPad: .left) }

  override var placePageAreaAffectDirections: MWMAvailableAreaAffectDirections { return alternative(iPhone: [], iPad: .left) }

  override var widgetsAreaAffectDirections: MWMAvailableAreaAffectDirections { return alternative(iPhone: [], iPad: .left) }
}
