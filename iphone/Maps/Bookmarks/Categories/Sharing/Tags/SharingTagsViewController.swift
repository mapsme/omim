protocol SharingTagsViewControllerDelegate: class {
  func tagsSelectionCompletedWithResult(_ tags: [MWMTag])
}

final class SharingTagsViewController: MWMViewController {
  
  weak var delegate: SharingTagsViewControllerDelegate?
  
  @IBOutlet weak var loadingTagsView: UIView!
  @IBOutlet private weak var progressView: UIView!
  private lazy var progress: MWMCircularProgress = {
    var p = MWMCircularProgress(parentView: progressView)
    return p
  }()
  
  @IBOutlet weak var collectionView: UICollectionView!
  
  let dataSource = TagsDataSource()
  
  let kTagCellIdentifier = "TagCellIdentifier"
  let kTagHeaderIdentifier = "TagHeaderIdentifier"
  
  override func viewDidLoad() {
    super.viewDidLoad()

    title = L("Select tags") //"Select tags"
    
    navigationItem.rightBarButtonItem = UIBarButtonItem(barButtonSystemItem: .done,
                                                        target: self,
                                                        action: #selector(onDone))
    navigationItem.leftBarButtonItem = UIBarButtonItem(barButtonSystemItem: .cancel,
                                                       target: self,
                                                       action: #selector(onCancel))
    
    collectionView.allowsMultipleSelection = true
    collectionView.contentInset = UIEdgeInsets(top: 10, left: 10, bottom: 20, right: 20)

    progress.state = .spinner
    dataSource.loadTags { success in
      self.progress.state = .completed
      self.loadingTagsView.isHidden = true
      if success {
        self.collectionView.reloadData()
      } else {
        //handle errors
      }
    }
  }
  
  @objc func onDone() {
    delegate?.tagsSelectionCompletedWithResult(dataSource.selectionResult())
    navigationController?.popViewController(animated: true)
  }
  
  @objc func onCancel() {
    navigationController?.popViewController(animated: true)
  }
 }

 extension SharingTagsViewController: UICollectionViewDataSource, UICollectionViewDelegate {

  func numberOfSections(in collectionView: UICollectionView) -> Int {
    return dataSource.tagGroupsCount
  }

  func collectionView(_ collectionView: UICollectionView, numberOfItemsInSection section: Int) -> Int {
    return dataSource.tagsCount(in: section)
  }

  func collectionView(_ collectionView: UICollectionView, cellForItemAt indexPath: IndexPath) -> UICollectionViewCell {
    let cell = collectionView.dequeueReusableCell(withReuseIdentifier: kTagCellIdentifier,for: indexPath) as! TagCollectionViewCell
    cell.update(with: dataSource.tag(in: indexPath.section, at: indexPath.item))
    
    if #available(iOS 12.0, *) {
      cell.layoutIfNeeded()
    }
  
    return cell
  }

  func collectionView(_ collectionView: UICollectionView,
                               viewForSupplementaryElementOfKind kind: String,
                               at indexPath: IndexPath) -> UICollectionReusableView {
    let supplementaryView = collectionView.dequeueReusableSupplementaryView(ofKind: kind,
                                                                            withReuseIdentifier: kTagHeaderIdentifier,
                                                                            for: indexPath) as! TagSectionHeaderView
    supplementaryView.update(with: dataSource.tagGroup(at: indexPath.section))

    return supplementaryView
  }
  
  func collectionView(_ collectionView: UICollectionView,
                               didSelectItemAt indexPath: IndexPath) {
    dataSource.selectTag(in: indexPath.section, at: indexPath.item)
  }
  
  func collectionView(_ collectionView: UICollectionView,
                      didDeselectItemAt indexPath: IndexPath) {
    dataSource.deselectTag(in: indexPath.section, at: indexPath.item)
  }
}

