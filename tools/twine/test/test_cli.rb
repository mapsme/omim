require 'twine_test_case'

class CLITestCase < TwineTestCase
  def setup
    super

    @strings_file_path = File.join @output_dir, SecureRandom.uuid
    @input_path = File.join @output_dir, SecureRandom.uuid
    @input_dir = @output_dir
  end

  def parse(command)
    @options = Twine::CLI::parse command.split
  end

  class TestValidateStringsFile < CLITestCase
    def test_command
      parse "validate-strings-file #{@strings_file_path}"

      assert_equal 'validate-strings-file', @options[:command]
      assert_equal @strings_file_path, @options[:strings_file]
    end

    def test_pedantic
      parse "validate-strings-file #{@strings_file_path} --pedantic"
      assert @options[:pedantic]
    end

    def test_missing_parameter
      assert_raises Twine::Error do
        parse 'validate-strings-file'
      end
    end

    def test_extra_parameter
      assert_raises Twine::Error do
        parse 'validate-strings-file strings extra'
      end
    end
  end

  class TestGenerateStringFile < CLITestCase
    def test_command
      parse "generate-string-file #{@strings_file_path} #{@output_path}"

      assert_equal 'generate-string-file', @options[:command]
      assert_equal @strings_file_path, @options[:strings_file]
      assert_equal @output_path, @options[:output_path]
    end

    def test_missing_parameter
      assert_raises Twine::Error do
        parse 'generate-string-file strings'
      end
    end

    def test_validate
      parse "generate-string-file #{@strings_file_path} #{@output_path} --validate"
      assert @options[:validate]
    end

    def test_extra_parameter
      assert_raises Twine::Error do
        parse 'generate-string-file strings output extra'
      end
    end

    def test_only_allows_one_language
      assert_raises Twine::Error do
        parse "generate-string-file strings output --lang en,fr"
      end
    end
  end

  class TestGenerateAllStringFiles < CLITestCase
    def test_command
      parse "generate-all-string-files #{@strings_file_path} #{@output_dir}"

      assert_equal 'generate-all-string-files', @options[:command]
      assert_equal @strings_file_path, @options[:strings_file]
      assert_equal @output_dir, @options[:output_path]
    end

    def test_missing_parameter
      assert_raises Twine::Error do
        parse "generate-all-string-files strings"
      end
    end

    def test_validate
      parse "generate-all-string-files #{@strings_file_path} #{@output_dir} --validate"
      assert @options[:validate]
    end

    def test_extra_parameter
      assert_raises Twine::Error do
        parse "generate-all-string-files strings output extra"
      end
    end
  end

  class TestConsumeStringFile < CLITestCase
    def test_command
      parse "consume-string-file #{@strings_file_path} #{@input_path}"

      assert_equal 'consume-string-file', @options[:command]
      assert_equal @strings_file_path, @options[:strings_file]
      assert_equal @input_path, @options[:input_path]
    end

    def test_missing_parameter
      assert_raises Twine::Error do
        parse "consume-string-file strings"
      end
    end

    def test_extra_parameter
      assert_raises Twine::Error do
        parse "consume-string-file strings output extra"
      end
    end

    def test_only_allows_one_language
      assert_raises Twine::Error do
        parse "consume-string-file strings output --lang en,fr"
      end
    end
  end

  class TestConsumeAllStringFiles < CLITestCase
    def test_command
      parse "consume-all-string-files #{@strings_file_path} #{@input_dir}"

      assert_equal 'consume-all-string-files', @options[:command]
      assert_equal @strings_file_path, @options[:strings_file]
      assert_equal @input_dir, @options[:input_path]
    end

    def test_missing_parameter
      assert_raises Twine::Error do
        parse "consume-all-string-files strings"
      end
    end

    def test_extra_parameter
      assert_raises Twine::Error do
        parse "consume-all-string-files strings output extra"
      end
    end
  end

  class TestGenerateLocDrop < CLITestCase
    def test_command
      parse "generate-loc-drop #{@strings_file_path} #{@output_path} --format apple"

      assert_equal 'generate-loc-drop', @options[:command]
      assert_equal @strings_file_path, @options[:strings_file]
      assert_equal @output_path, @options[:output_path]
    end

    def test_missing_parameter
      assert_raises Twine::Error do
        parse "generate-loc-drop strings --format apple"
      end
    end

    def test_validate
      parse "generate-loc-drop #{@strings_file_path} #{@output_path} --format apple --validate"
      assert @options[:validate]
    end

    def test_extra_parameter
      assert_raises Twine::Error do
        parse "generate-loc-drop strings output extra --format apple"
      end
    end

    def test_format_needed
      assert_raises Twine::Error do
        parse "generate-loc-drop strings output"
      end
    end
  end

  class TestConsumeLocDrop < CLITestCase
    def test_command
      parse "consume-loc-drop #{@strings_file_path} #{@input_path}"

      assert_equal 'consume-loc-drop', @options[:command]
      assert_equal @strings_file_path, @options[:strings_file]
      assert_equal @input_path, @options[:input_path]
    end

    def test_missing_parameter
      assert_raises Twine::Error do
        parse "consume-loc-drop strings"
      end
    end

    def test_extra_parameter
      assert_raises Twine::Error do
        parse "consume-loc-drop strings input extra"
      end
    end
  end

  class TestParameters < CLITestCase
    def parse_with(parameter)
      parse 'validate-strings-file input.txt ' + parameter
    end

    def test_default_options
      parse_with ''
      expected = {command: 'validate-strings-file', strings_file: 'input.txt', include: :all}
      assert_equal expected, @options
    end

    def test_create_folders
      parse_with '--create-folders'
      assert @options[:create_folders]
    end

    def test_consume_all
      parse_with '--consume-all'
      assert @options[:consume_all]
    end

    def test_consume_comments
      parse_with '--consume-comments'
      assert @options[:consume_comments]
    end

    def test_untagged
      parse_with '--untagged'
      assert @options[:untagged]
    end

    def test_developer_language
      random_language = KNOWN_LANGUAGES.sample
      parse_with "--developer-lang #{random_language}"
      assert_equal random_language, @options[:developer_language]
    end

    def test_single_language
      random_language = KNOWN_LANGUAGES.sample
      parse_with "--lang #{random_language}"
      assert_equal [random_language], @options[:languages]
    end

    def test_multiple_languages
      random_languages = KNOWN_LANGUAGES.shuffle[0, 3]
      parse_with "--lang #{random_languages.join(',')}"
      assert_equal random_languages.sort, @options[:languages].sort
    end

    def test_single_tag
      random_tag = "tag#{rand(100)}"
      parse_with "--tags #{random_tag}"
      assert_equal [random_tag], @options[:tags]
    end

    def test_multiple_tags
      random_tags = ([nil] * 3).map { "tag#{rand(100)}" }
      parse_with "--tags #{random_tags.join(',')}"
      assert_equal random_tags.sort, @options[:tags].sort
    end

    def test_format
      random_format = Twine::Formatters.formatters.sample.format_name.downcase
      parse_with "--format #{random_format}"
      assert_equal random_format, @options[:format]
    end

    def test_include
      random_set = [:all, :translated, :untranslated].sample
      parse_with "--include #{random_set}"
      assert_equal random_set, @options[:include]
    end

    def test_output_path
      parse_with "--output-file #{@output_path}"
      assert_equal @output_path, @options[:output_path]
    end

    def test_file_name
      random_filename = "#{rand(10000)}"
      parse_with "--file-name #{random_filename}"
      assert_equal random_filename, @options[:file_name]
    end

    def test_encoding
      parse_with '--encoding UTF16'
      assert_equal 'UTF16', @options[:output_encoding]
    end
  end
end
