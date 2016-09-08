require 'erb'
require 'minitest/autorun'
require "mocha/mini_test"
require 'securerandom'
require 'stringio'
require 'twine'
require 'twine_file_dsl'

class TwineTestCase < Minitest::Test
  include TwineFileDSL

  KNOWN_LANGUAGES = %w(en fr de es)
  
  def setup
    super
    Twine::stdout = StringIO.new
    Twine::stderr = StringIO.new

    @formatters = Twine::Formatters.formatters.dup

    @output_dir = Dir.mktmpdir
    @output_path = File.join @output_dir, SecureRandom.uuid
  end

  def teardown
    FileUtils.remove_entry_secure @output_dir if File.exists? @output_dir
    Twine::Formatters.formatters.clear
    Twine::Formatters.formatters.concat @formatters
    super
  end

  def execute(command)
    command += "  -o #{@output_path}"
    Twine::Runner.run(command.split(" "))
  end

  def fixture_path(filename)
    File.join File.dirname(__FILE__), 'fixtures', filename
  end

  def content(filename)
    ERB.new(File.read fixture_path(filename)).result
  end

  def content_io(filename)
    StringIO.new ERB.new(File.read fixture_path(filename)).result
  end
end
