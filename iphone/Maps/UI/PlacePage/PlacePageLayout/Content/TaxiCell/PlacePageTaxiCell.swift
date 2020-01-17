@objc(MWMPlacePageTaxiCell)
final class PlacePageTaxiCell: MWMTableViewCell {
  @IBOutlet private weak var icon: UIImageView!
  @IBOutlet private weak var title: UILabel!
  @IBOutlet private weak var orderButton: UIButton!

  private weak var delegate: MWMPlacePageButtonsProtocol!
  private var type: MWMPlacePageTaxiProvider!

  @objc func config(type: MWMPlacePageTaxiProvider, delegate: MWMPlacePageButtonsProtocol) {
    self.delegate = delegate
    self.type = type
    switch type {
    case .taxi:
      icon.image = #imageLiteral(resourceName: "icTaxiTaxi")
      title.text = L("taxi")
    case .uber:
      icon.image = #imageLiteral(resourceName: "icTaxiUber")
      title.text = L("uber")
    case .yandex:
      icon.image = #imageLiteral(resourceName: "ic_taxi_logo_yandex")
      title.text = L("yandex_taxi_title")
    case .maxim:
      icon.image = #imageLiteral(resourceName: "ic_taxi_logo_maksim")
      title.text = L("maxim_taxi_title")
    case .vezet:
      icon.image = #imageLiteral(resourceName: "ic_taxi_logo_vezet")
      title.text = L("vezet_taxi")
    }
  }

  @IBAction func orderAction() {
    delegate.orderTaxi(type)
  }
}
