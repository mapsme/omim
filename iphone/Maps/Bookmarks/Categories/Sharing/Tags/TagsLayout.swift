final class TagsLayout: UICollectionViewLayout {
  enum Element: String {
    case header
    case cell
  }

  private var cache = [Element : [IndexPath : UICollectionViewLayoutAttributes]]()
  fileprivate var contentHeight: CGFloat = 0

  fileprivate var contentWidth: CGFloat {
    guard let collectionView = collectionView else {
      return 0
    }
    
    let insets = collectionView.contentInset
    return collectionView.bounds.width - (insets.left + insets.right)
  }

  override var collectionViewContentSize: CGSize {
    return CGSize(width: contentWidth, height: contentHeight)
  }
  
  let itemHeight: CGFloat = 50
  let itemWidth: CGFloat = 50
  let lineSpacing: CGFloat = 10
  let elementsSpacing: CGFloat = 10

  override func prepare() {
    super.prepare()
    
    guard let collectionView = collectionView else {
        return
    }
    
    if cache.isEmpty {
      cache[.header] = [IndexPath: UICollectionViewLayoutAttributes]()
      cache[.cell] = [IndexPath: UICollectionViewLayoutAttributes]()
    }
    
    var xOffset: CGFloat = 0
    var yOffset: CGFloat = 0
    contentHeight = 0
    var lastItemHeight: CGFloat = 0
    
    for section in 0 ..< collectionView.numberOfSections {
      yOffset += lastItemHeight + 2 * lineSpacing
      let indexPath = IndexPath(item: 0, section: section)
      
      let headerSize = cache[.header]?[indexPath]?.size ?? CGSize(width: contentWidth, height: itemHeight)
      let frame = CGRect(x: 0, y: yOffset, width: headerSize.width, height: headerSize.height)
      let attr = UICollectionViewLayoutAttributes(forSupplementaryViewOfKind: UICollectionElementKindSectionHeader, with: indexPath)
      attr.frame = frame
      cache[.header]?[indexPath] = attr
      
      yOffset += headerSize.height + lineSpacing
      xOffset = 0
      
      for item in 0 ..< collectionView.numberOfItems(inSection: section) {
        let indexPath = IndexPath(item: item, section: section)
        let itemSize = cache[.cell]?[indexPath]?.size ?? CGSize(width: itemWidth, height: itemHeight)
        
        if xOffset + itemSize.width > contentWidth {
          xOffset = 0
          yOffset = yOffset + lineSpacing + lastItemHeight
        }
        
        let frame = CGRect(x: xOffset, y: yOffset, width: itemSize.width, height: itemSize.height)
        let attr = UICollectionViewLayoutAttributes(forCellWith: indexPath)
        attr.frame = frame
        cache[.cell]?[indexPath] = attr
        
        xOffset += itemSize.width + elementsSpacing
        lastItemHeight = itemSize.height
        
        contentHeight = max(contentHeight, frame.maxY)
      }
    }
  }

  override func layoutAttributesForElements(in rect: CGRect) -> [UICollectionViewLayoutAttributes]? {
    var visibleLayoutAttributes = [UICollectionViewLayoutAttributes]()
    
    for (_, elementInfo) in cache {
      for (_, attributes) in elementInfo {
        if attributes.frame.intersects(rect) {
          visibleLayoutAttributes.append(attributes)
        }
      }
    }

    return visibleLayoutAttributes
  }
  
  override public func shouldInvalidateLayout(forPreferredLayoutAttributes preferredAttributes: UICollectionViewLayoutAttributes,
                                              withOriginalAttributes originalAttributes: UICollectionViewLayoutAttributes) -> Bool {
    //dont validate layout if original width already equals to contentWidth - it's the best deal we can offer to cell
    if preferredAttributes.size.height != originalAttributes.size.height ||
      (preferredAttributes.size.width != originalAttributes.size.width && originalAttributes.size.width < contentWidth) {
      if preferredAttributes.representedElementKind == UICollectionElementKindSectionHeader {
        cache[.header]?[originalAttributes.indexPath]?.size = preferredAttributes.size
      } else {
        cache[.cell]?[originalAttributes.indexPath]?.size = CGSize(width: min(preferredAttributes.size.width, contentWidth),
                                                                   height: preferredAttributes.size.height)
      }
      return true
    }
    return false
  }
}
