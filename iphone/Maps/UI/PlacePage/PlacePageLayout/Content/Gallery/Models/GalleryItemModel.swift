@objc(MWMGalleryItemModel)
final class GalleryItemModel: NSObject {
  @objc let imageURL: URL
  @objc let previewURL: URL

  @objc init(imageURL: URL, previewURL: URL) {
    self.imageURL = imageURL
    self.previewURL = previewURL
  }
}
