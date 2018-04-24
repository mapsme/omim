import Foundation
import Alamofire

final class MultipartUploader: NSObject {
  static var manager: Alamofire.SessionManager = {
    let configuration = URLSessionConfiguration.default
    configuration.timeoutIntervalForRequest = 5
    return Alamofire.SessionManager(configuration: configuration)
  }()
  
  @objc
  static func upload(method: String, url: String, fileKey: String, filePath: String, params: [String: String], headers: [String: String], callback: @escaping (Int, String) -> Void) {

    let multipartFormData: (Alamofire.MultipartFormData) -> Void = { mfd in
      for (key, value) in params {
        mfd.append(value.data(using: .utf8, allowLossyConversion: false)!, withName: key)
      }
      mfd.append(URL(fileURLWithPath: filePath), withName: fileKey)
    }

    let method = HTTPMethod(rawValue: method) ?? .post
    
    manager.upload(multipartFormData: multipartFormData, to: url, method: method, headers: headers) { encodingResult in
      switch encodingResult {
      case let .success(upload, _, _):
        upload.responseJSON { callback($0.response?.statusCode ?? -1, $0.error?.localizedDescription ?? "") }
      case let .failure(encodingError):
        callback(-1, encodingError.localizedDescription)
      }
    }
  }
}
