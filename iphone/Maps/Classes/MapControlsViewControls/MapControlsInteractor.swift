final class MapControlsInteractor {
}

extension MapControlsInteractor: IMapControlsInteractor {
  func setPlacePageSelectedCallback(_ selected: @escaping MapObjectSelectedClosure,
                                    deselectedCallback: @escaping MapObjectDeselectedClosure,
                                    updatedCallback: @escaping MapObjectUpdatedClosure) {
    FrameworkHelper.setPlacePageSelectedCallback(selected,
                                                 deselectedCallback: deselectedCallback,
                                                 updatedCallback: updatedCallback)
  }

  func zoomIn() {
    FrameworkHelper.zoomMap(.in)
  }

  func zoomOut() {
    FrameworkHelper.zoomMap(.out)
  }

  func switchMyPositionMode() {
    FrameworkHelper.switchMyPositionMode()
  }
}
