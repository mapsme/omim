@objc(MWMViatorItemModel)
final class ViatorItemModel: NSObject {
  @objc let imageURL: URL
  @objc let pageURL: URL
  @objc let title: String
  @objc let rating: Double
  @objc let duration: String
  @objc let price: String

  @objc init(imageURL: URL, pageURL: URL, title: String, rating: Double, duration: String, price: String) {
    self.imageURL = imageURL
    self.pageURL = pageURL
    self.title = title
    self.rating = rating
    self.duration = duration
    self.price = price
  }
}
