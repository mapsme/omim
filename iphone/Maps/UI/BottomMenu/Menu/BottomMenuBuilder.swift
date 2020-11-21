@objc class BottomMenuBuilder: NSObject {
  @objc static func buildMenu(mapViewController: MapViewController,
                              delegate: BottomMenuDelegate) -> UIViewController {
    return BottomMenuBuilder.build(mapViewController: mapViewController,
                                   delegate: delegate,
                                   sections: [.layers, .items],
                                   source: kStatMenu)
  }

  @objc static func buildLayers(mapViewController: MapViewController,
                                delegate: BottomMenuDelegate) -> UIViewController {
    return BottomMenuBuilder.build(mapViewController: mapViewController,
                                   delegate: delegate,
                                   sections: [.layers],
                                   source: kStatMap)
  }

  private static func build(mapViewController: MapViewController,
                            delegate: BottomMenuDelegate,
                            sections: [BottomMenuPresenter.Sections],
                            source: String) -> UIViewController {
    let viewController = BottomMenuViewController(nibName: nil, bundle: nil)
    let interactor = BottomMenuInteractor(viewController: viewController,
                                          mapViewController: mapViewController,
                                          delegate: delegate)
    let presenter = BottomMenuPresenter(view: viewController, interactor: interactor, sections: sections, source: source)
    
    interactor.presenter = presenter
    viewController.presenter = presenter
    
    return viewController
  }
}
