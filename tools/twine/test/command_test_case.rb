require 'twine_test_case'

class CommandTestCase < TwineTestCase
  def prepare_mock_formatter(formatter_class)
    strings = Twine::StringsFile.new
    strings.language_codes.concat KNOWN_LANGUAGES

    formatter = formatter_class.new
    formatter.strings = strings
    Twine::Formatters.formatters.clear
    Twine::Formatters.formatters << formatter
    formatter
  end
end
