# encoding: utf-8

require 'command_test_case'

class TestValidateStringsFile < CommandTestCase
  def setup
    super
    @options = { strings_file: 'input.txt' }

    @twine_file = build_twine_file 'en' do
      add_section 'Section 1' do
        add_row key1: 'value1', tags: ['tag1']
        add_row key2: 'value2', tags: ['tag1']
      end

      add_section 'Section 2' do
        add_row key3: 'value3', tags: ['tag1', 'tag2']
        add_row key4: 'value4', tags: ['tag2']
      end
    end
  end

  def random_row
    @twine_file.strings_map[@twine_file.strings_map.keys.sample]
  end

  def test_recognizes_valid_file
    Twine::Runner.new(@options, @twine_file).validate_strings_file
    assert_equal "input.txt is valid.\n", Twine::stdout.string
  end

  def test_reports_duplicate_keys
    @twine_file.sections[0].rows << random_row

    assert_raises Twine::Error do
      Twine::Runner.new(@options, @twine_file).validate_strings_file
    end
  end

  def test_reports_invalid_characters_in_keys
    random_row.key[0] = "!?;:,^`´'\"\\|/(){}[]~-+*=#$%".chars.to_a.sample

    assert_raises Twine::Error do
      Twine::Runner.new(@options, @twine_file).validate_strings_file
    end
  end

  def test_does_not_reports_missing_tags_by_default
    random_row.tags.clear

    Twine::Runner.new(@options, @twine_file).validate_strings_file
  end

  def test_reports_missing_tags
    random_row.tags.clear

    assert_raises Twine::Error do
      Twine::Runner.new(@options.merge(pedantic: true), @twine_file).validate_strings_file
    end
  end
end
