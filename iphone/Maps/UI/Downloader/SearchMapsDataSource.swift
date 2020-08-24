class SearchMapsDataSource {
  typealias SearchCallback = (Bool) -> Void

  fileprivate var searchResults: [MapSearchResult] = []
  fileprivate var searchId = 0
  fileprivate var onUpdate: SearchCallback?
  fileprivate var query: String = ""
  fileprivate var lastSuccessfulQuery: String = ""
}

extension SearchMapsDataSource: IDownloaderDataSource {
  var isEmpty: Bool {
    searchResults.isEmpty
  }

  var title: String {
    ""
  }

  var isRoot: Bool {
    true
  }

  var isSearching: Bool {
    true
  }

  func numberOfSections() -> Int {
    1
  }

  func parentAttributes() -> MapNodeAttributes {
    return Storage.shared().attributesForRoot()
  }

  func numberOfItems(in section: Int) -> Int {
    searchResults.count
  }

  func item(at indexPath: IndexPath) -> MapNodeAttributes {
    precondition(indexPath.row < searchResults.count, "Search error: index:\(indexPath.row), count: \(searchResults.count), query: \(query) / \(lastSuccessfulQuery)")
    return Storage.shared().attributes(forCountry: searchResults[indexPath.item].countryId)
  }

  func matchedName(at indexPath: IndexPath) -> String? {
    searchResults[indexPath.item].matchedName
  }

  func title(for section: Int) -> String {
    L("downloader_search_results")
  }

  func indexTitles() -> [String]? {
    nil
  }

  func dataSourceFor(_ childId: String) -> IDownloaderDataSource {
    AvailableMapsDataSource(childId)
  }

  func reload(_ completion: () -> Void) {
    completion()
  }

  func search(_ query: String, locale: String, update: @escaping SearchCallback) {
    searchId += 1
    onUpdate = update
    self.query = query
    FrameworkHelper.search(inDownloader: query, inputLocale: locale) { [weak self, searchId] (results, finished) in
      if searchId != self?.searchId {
        return
      }
      self?.searchResults = results
      self?.lastSuccessfulQuery = query
      if !results.isEmpty || finished {
        self?.onUpdate?(finished)
      }
    }
  }

  func cancelSearch() {
    searchResults = []
    onUpdate = nil
  }
}
