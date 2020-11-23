final class MapControlsInteractor {
  private var locationModeCallback: MyPositionModeChangedClosure?

  init() {
    FrameworkHelper.shared().addLocationModeListener(self)
  }

  deinit {
    FrameworkHelper.shared().removeLocationModeListener(self)
  }
}

extension MapControlsInteractor: LocationModeListener {
  func processMyPositionStateModeEvent(_ mode: MyPositionMode) {
    locationModeCallback?(mode)
  }

  func processMyPositionPendingTimeout() {}
}

extension MapControlsInteractor: IMapControlsInteractor {
  func setPlacePageSelectedCallback(_ selected: @escaping MapObjectSelectedClosure,
                                    deselectedCallback: @escaping MapObjectDeselectedClosure,
                                    updatedCallback: @escaping MapObjectUpdatedClosure) {
    FrameworkHelper.shared().setPlacePageSelectedCallback(selected,
                                                 deselectedCallback: deselectedCallback,
                                                 updatedCallback: updatedCallback)
  }

  func setMyPositionModeCallback(_ callback: @escaping MyPositionModeChangedClosure) {
    locationModeCallback = callback
  }

  func zoomIn() {
    FrameworkHelper.shared().zoomMap(.in)
  }

  func zoomOut() {
    FrameworkHelper.shared().zoomMap(.out)
  }

  func switchMyPositionMode() {
    FrameworkHelper.shared().switchMyPositionMode()
  }
}
