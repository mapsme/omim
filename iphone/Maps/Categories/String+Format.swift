extension String {
  init(coreFormat: String, arguments: [CVarArg]) {
    let format = coreFormat.replacingOccurrences(of: "%s", with: "%@")
    self.init(format: format, arguments: arguments.map { "\($0)" })
  }
}

struct CoreFormat<T: CVarArg> {
  static func get(_ coreFormat: String, arguments: [T]) -> String {
    let format: String
    switch T.self {
    case is UInt.Type:
      fallthrough
    case is Int.Type:
      format = coreFormat.replacingOccurrences(of: "%s", with: "%d")
    case is Double.Type:
      fallthrough
    case is Float.Type:
      format = coreFormat.replacingOccurrences(of: "%s", with: "%f")
    default:
      format = coreFormat.replacingOccurrences(of: "%s", with: "%@")
    }
    return String(format: format, arguments: arguments)
  }
}
