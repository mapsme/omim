protocol ElevationProfileViewProtocol: class {
  var presenter: ElevationProfilePresenterProtocol?  { get set }
  
  var isExtendedDifficultyLabelHidden: Bool { get set }
  func setExtendedDifficultyGrade(_ value: String)
  func setTrackTime(_ value: String?)
  func setDifficulty(_ value: ElevationDifficulty)
}

class ElevationProfileViewController: UIViewController {
  var presenter: ElevationProfilePresenterProtocol?
  
  @IBOutlet private var graphViewContainer: UIView!
  @IBOutlet private var descriptionCollectionView: UICollectionView!
  @IBOutlet private var difficultyView: DifficultyView!
  @IBOutlet private var extendedDifficultyGradeLabel: UILabel!
  @IBOutlet private var trackTimeLabel: UILabel!
  @IBOutlet private var extendedGradeButton: UIButton!
  
  override func viewDidLoad() {
    super.viewDidLoad()
    descriptionCollectionView.dataSource = presenter
    descriptionCollectionView.delegate = presenter
    presenter?.configure()
  }

  override func viewDidAppear(_ animated: Bool) {
    super.viewDidAppear(animated)
    presenter?.onAppear()
  }

  override func viewDidDisappear(_ animated: Bool) {
    super.viewDidDisappear(animated)
    presenter?.onDissapear()
  }

  @IBAction func onExtendedDifficultyButtonPressed(_ sender: Any) {
    presenter?.onDifficultyButtonPressed()
  }

  func getPreviewHeight() -> CGFloat {
    return view.height - descriptionCollectionView.frame.minY
  }
}

extension ElevationProfileViewController: ElevationProfileViewProtocol {
  var isExtendedDifficultyLabelHidden: Bool {
    get { return extendedDifficultyGradeLabel.isHidden }
    set {
      extendedDifficultyGradeLabel.isHidden = newValue
      extendedGradeButton.isHidden = newValue
    }
  }
  
  func setExtendedDifficultyGrade(_ value: String) {
    extendedDifficultyGradeLabel.text = value
  }
  func setTrackTime(_ value: String?) {
    trackTimeLabel.text = value
  }
  func setDifficulty(_ value: ElevationDifficulty) {
    difficultyView.difficulty = value
  }
}
