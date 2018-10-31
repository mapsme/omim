protocol UploadActionCellDelegate: AnyObject {
  func cellDidPressShareButton(_ cell: UploadActionCell)
}

enum UploadActionCellState :String {
  case normal, inProgress, completed
}

final class UploadActionCell: MWMTableViewCell {
  @IBOutlet private weak var actionImage: UIImageView!
  @IBOutlet private weak var actionTitle: UILabel!
  @IBOutlet private weak var progressView: UIView!
  
  weak var delegate: UploadActionCellDelegate?
  private var titles: [UploadActionCellState : String]?
  private lazy var progress: MWMCircularProgress = {
    var p = MWMCircularProgress(parentView: self.progressView)
    p.delegate = self
    p.setSpinnerImage("ic24PxShare", for: .completed)
    return p
  }()
  
  func config(titles: [UploadActionCellState : String], image: UIImage, delegate: UploadActionCellDelegate?) {
    actionImage.image = image
    self.titles = titles
    self.delegate = delegate
    self.changeCellState(.normal)
  }
  
  func changeCellState(_ state: UploadActionCellState) {
    switch state {
    case .normal:
      progress.state = .normal
      actionImage.tintColor = .linkBlue()
      actionTitle.textColor = .linkBlue()
      actionTitle.font = .regular16()
      actionTitle.text = titles?[.normal]
      break
    case .inProgress:
      progress.state = .spinner
      actionImage.tintColor = .sharingInProgress()
      actionTitle.textColor = .sharingInProgress()
      actionTitle.font = .italic16()
      actionTitle.text = titles?[.inProgress]
      break
    case .completed:
      progress.state = .completed
      actionImage.tintColor = .sharingInProgress()
      actionTitle.textColor = .sharingInProgress()
      actionTitle.font = .regular16()
      actionTitle.text = titles?[.completed]
      break
    }
  }
}

extension UploadActionCell: MWMCircularProgressProtocol {
  func progressButtonPressed(_ progress: MWMCircularProgress) {
    if (progress.state == .completed) {
      delegate?.cellDidPressShareButton(self)
    }
  }
}
