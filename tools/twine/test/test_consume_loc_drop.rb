require 'command_test_case'

class TestConsumeLocDrop < CommandTestCase
  def setup
    super

    options = {}
    options[:input_path] = fixture_path 'consume_loc_drop.zip'
    options[:output_path] = @output_path
    options[:format] = 'apple'

    @twine_file = build_twine_file 'en', 'es' do
      add_section 'Section' do
        add_row key1: 'value1'
      end
    end

    @runner = Twine::Runner.new(options, @twine_file)
  end

  def test_consumes_zip_file
    @runner.consume_loc_drop

    assert @twine_file.strings_map['key1'].translations['en'], 'value1-english'
    assert @twine_file.strings_map['key1'].translations['es'], 'value1-spanish'
  end
end
