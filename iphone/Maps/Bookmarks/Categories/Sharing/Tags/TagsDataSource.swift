final class TagsDataSource: NSObject {
  
  private var tagGroups: [MWMTagGroup] = []
  private var selectedTags: [MWMTag] = []
  
  func loadTags(onComplete: @escaping (Bool) -> Void) {
    MWMBookmarksManager.shared().loadTags { (success, tags) in
      self.tagGroups = tags
      onComplete(success)
    }
  }
  
  var tagGroupsCount: NSInteger {
    get {
      return tagGroups.count
    }
  }
  
  func tagGroup(at index: Int) -> MWMTagGroup {
    return tagGroups[index]
  }
  
  func tagsCount(in groupIndex: Int) -> Int {
    return tagGroups[groupIndex].tags.count
  }
  
  func tag(in groupIndex: Int, at index: Int) -> MWMTag {
    return tagGroups[groupIndex].tags[index]
  }
  
  func selectTag(in groupIndex: Int, at index: Int) {
    selectedTags.append(tagGroups[groupIndex].tags[index])
  }
  
  func deselectTag(in groupIndex: Int, at index: Int) {
    if let index = selectedTags.index(of: tagGroups[groupIndex].tags[index]) {
      selectedTags.remove(at: index)
    }
  }
  
  func selectionResult() -> [MWMTag] {
    return selectedTags
  }
}
