require 'command_test_case'

class TestGenerateLocDrop < CommandTestCase
  def new_runner(twine_file = nil)
    options = {}
    options[:output_path] = @output_path
    options[:format] = 'apple'

    unless twine_file
      twine_file = build_twine_file 'en', 'fr' do
        add_section 'Section' do
          add_row key: 'value'
        end
      end
    end

    Twine::Runner.new(options, twine_file)
  end

  def test_generates_zip_file
    new_runner.generate_loc_drop

    assert File.exists?(@output_path), "zip file should exist"
  end

  def test_zip_file_structure
    new_runner.generate_loc_drop

    names = []
    Zip::File.open(@output_path) do |zipfile|
      zipfile.each do |entry|
        names << entry.name
      end
    end
    assert_equal ['Locales/', 'Locales/en.strings', 'Locales/fr.strings'], names
  end

  def test_uses_formatter
    formatter = prepare_mock_formatter Twine::Formatters::Apple
    formatter.expects(:format_file).twice

    new_runner.generate_loc_drop
  end

  def test_prints_empty_file_warnings
    empty_twine_file = build_twine_file('en') {}
    new_runner(empty_twine_file).generate_loc_drop
    assert_match "Skipping file", Twine::stderr.string
  end

  class TestValidate < CommandTestCase
    def new_runner(validate)
      options = {}
      options[:output_path] = @output_path
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

      new_runner(false).generate_loc_drop
    end

    def test_validates_strings_file_if_validate
      assert_raises Twine::Error do
        new_runner(true).generate_loc_drop
      end
    end
  end
end
