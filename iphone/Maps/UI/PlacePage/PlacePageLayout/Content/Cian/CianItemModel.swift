@objc(MWMCianItemModel)
final class CianItemModel: NSObject {
  @objc let roomsCount: UInt
  @objc let priceRur: UInt
  @objc let pageURL: URL
  @objc let address: String

  @objc init(roomsCount: UInt, priceRur: UInt, pageURL: URL, address: String) {
    self.roomsCount = roomsCount
    self.priceRur = priceRur
    self.pageURL = pageURL
    self.address = address
  }
}
