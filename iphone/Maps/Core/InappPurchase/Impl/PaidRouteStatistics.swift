class PaidRouteStatistics: IPaidRouteStatistics {
  let serverId: String
  let productId: String
  let vendor: String
  let testGroup: String

  init(serverId: String,
       productId: String,
       vendor: String,
       testGroup: String) {
    self.serverId = serverId
    self.productId = productId
    self.vendor = vendor
    self.testGroup = testGroup
  }

  func logPreviewShow() {
    logEvent(kStatInappShow, withParameters: [kStatVendor: vendor,
                                              kStatProduct: productId,
                                              kStatPurchase: serverId,
                                              kStatTestGroup: testGroup],
             withChannel: .realtime)
  }

  func logPay() {
    logEvent(kStatInappSelect, withParameters: [kStatPurchase: serverId, kStatProduct: productId])
    logEvent(kStatInappPay, withParameters: [kStatPurchase: serverId], withChannel: .realtime)
  }

  func logCancel() {
    logEvent(kStatInappCancel, withParameters: [kStatPurchase: serverId])
  }

  func logPaymentSuccess() {
    logEvent(kStatInappPaymentSuccess, withParameters: [kStatPurchase: serverId])
  }

  func logPaymentError(_ errorText: String) {
    logEvent(kStatInappPaymentError, withParameters: [kStatPurchase: serverId,
                                                      kStatError: errorText])
  }

  func logValidationSuccess() {
    logEvent(kStatInappValidationSuccess, withParameters: [kStatPurchase: serverId])
  }

  func logValidationError(_ code: Int) {
    logEvent(kStatInappValidationSuccess, withParameters: [kStatPurchase: serverId,
                                                           kStatErrorCode: code])
  }
  
  private func logEvent(_ eventName: String, withParameters: [String: Any],
                        withChannel: StatisticsChannel = .default) {
    Statistics.logEvent(eventName, withParameters: withParameters, with: withChannel)
  }
}
