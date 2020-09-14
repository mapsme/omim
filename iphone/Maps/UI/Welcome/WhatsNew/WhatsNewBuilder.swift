class WhatsNewBuilder {
  static var configs:[WhatsNewPresenter.WhatsNewConfig] {
    return [
      WhatsNewPresenter.WhatsNewConfig(image: UIImage(named: "img_whatsnew_trial.png"),
                                       title: "whatsnew_trial_title",
                                       text: "whatsnew_trial_message",
                                       buttonNextTitle: "whatsnew_trial_cta",
                                       isCloseButtonHidden: false,
                                       action: {
                                        let subscribeViewController = SubscriptionViewBuilder.build(type: .allPass,
                                                                                                    parentViewController: MapViewController.shared(),
                                                                                                    source: kStatWhatsNew,
                                                                                                    successDialog: .goToCatalog,
                                                                                                    completion: nil)
                                        MapViewController.shared().present(subscribeViewController, animated: true)
      }),
      WhatsNewPresenter.WhatsNewConfig(image: UIImage(named: "img_whatsnew_trial.png"),
                                       title: "whatsnew_trial_title",
                                       text: "whatsnew_trial_message",
                                       buttonNextTitle: "done")
    ]
  }

  static func build(delegate: WelcomeViewDelegate) -> [UIViewController] {
    return WhatsNewBuilder.configs.map { (config) -> UIViewController in
      let sb = UIStoryboard.instance(.welcome)
      let vc = sb.instantiateViewController(ofType: WelcomeViewController.self);

      let router = WelcomeRouter(viewController: vc, delegate: delegate)
      let presenter = WhatsNewPresenter(view: vc,
                                        router: router,
                                        config: config)
      vc.presenter = presenter

      return vc
    }
	}
}
