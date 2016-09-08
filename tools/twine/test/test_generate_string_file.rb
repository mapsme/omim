require 'command_test_case'

class TestGenerateStringFile < CommandTestCase
  def new_runner(language, file)
    options = {}
    options[:output_path] = File.join(@output_dir, file) if file
    options[:languages] = language if language

    strings = Twine::StringsFile.new
    strings.language_codes.concat KNOWN_LANGUAGES

    Twine::Runner.new(options, strings)
  end

  def prepare_mock_format_file_formatter(formatter_class)
    formatter = prepare_mock_formatter(formatter_class)
    formatter.expects(:format_file).returns(true)
  end

  def test_deducts_android_format_from_output_path
    prepare_mock_format_file_formatter Twine::Formatters::Android

    new_runner('fr', 'fr.xml').generate_string_file
  end

  def test_deducts_apple_format_from_output_path
    prepare_mock_format_file_formatter Twine::Formatters::Apple

    new_runner('fr', 'fr.strings').generate_string_file
  end

  def test_deducts_jquery_format_from_output_path
    prepare_mock_format_file_formatter Twine::Formatters::JQuery

    new_runner('fr', 'fr.json').generate_string_file
  end

  def test_deducts_gettext_format_from_output_path
    prepare_mock_format_file_formatter Twine::Formatters::Gettext

    new_runner('fr', 'fr.po').generate_string_file
  end

  def test_deducts_language_from_output_path
    random_language = KNOWN_LANGUAGES.sample
    formatter = prepare_mock_formatter Twine::Formatters::Android
    formatter.expects(:format_file).with(random_language).returns(true)

    new_runner(nil, "#{random_language}.xml").generate_string_file
  end

  def test_returns_error_if_nothing_written
    formatter = prepare_mock_formatter Twine::Formatters::Android
    formatter.expects(:format_file).returns(false)

    assert_raises Twine::Error do
      new_runner('fr', 'fr.xml').generate_string_file
    end
  end

  class TestValidate < CommandTestCase
    def new_runner(validate)
      options = {}
      options[:output_path] = @output_path
      options[:languages] = ['en']
      options[:format] = 'android'
      options[:validate] = validate

      twine_file = build_twine_file 'en' do
        add_section 'Section' do
          add_row key: 'value'
          add_row key: 'value'
        end
      end

      Twine::Runner.new(options, twine_file)
    end

    def test_does_not_validate_strings_file
      prepare_mock_formatter Twine::Formatters::Android

      new_runner(false).generate_string_file
    end

    def test_validates_strings_file_if_validate
      assert_raises Twine::Error do
        new_runner(true).generate_string_file
      end
    end
  end
end
