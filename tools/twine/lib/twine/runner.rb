require 'tmpdir'
require 'fileutils'

Twine::Plugin.new # Initialize plugins first in Runner.

module Twine
  class Runner
    def self.run(args)
      options = CLI.parse(args)
      
      strings = StringsFile.new
      strings.read options[:strings_file]
      runner = new(options, strings)

      case options[:command]
      when 'generate-string-file'
        runner.generate_string_file
      when 'generate-all-string-files'
        runner.generate_all_string_files
      when 'consume-string-file'
        runner.consume_string_file
      when 'consume-all-string-files'
        runner.consume_all_string_files
      when 'generate-loc-drop'
        runner.generate_loc_drop
      when 'consume-loc-drop'
        runner.consume_loc_drop
      when 'validate-strings-file'
        runner.validate_strings_file
      end
    end

    def initialize(options = {}, strings = StringsFile.new)
      @options = options
      @strings = strings
    end

    def write_strings_data(path)
      if @options[:developer_language]
        @strings.set_developer_language_code(@options[:developer_language])
      end
      @strings.write(path)
    end

    def generate_string_file
      validate_strings_file if @options[:validate]

      lang = nil
      lang = @options[:languages][0] if @options[:languages]

      formatter, lang = prepare_read_write(@options[:output_path], lang)
      output = formatter.format_file(lang)

      raise Twine::Error.new "Nothing to generate! The resulting file would not contain any strings." unless output

      IO.write(@options[:output_path], output, encoding: encoding)
    end

    def generate_all_string_files
      validate_strings_file if @options[:validate]

      if !File.directory?(@options[:output_path])
        if @options[:create_folders]
          FileUtils.mkdir_p(@options[:output_path])
        else
          raise Twine::Error.new("Directory does not exist: #{@options[:output_path]}")
        end
      end

      formatter_for_directory = find_formatter { |f| f.can_handle_directory?(@options[:output_path]) }
      formatter = formatter_for_format(@options[:format]) || formatter_for_directory
      
      unless formatter
        raise Twine::Error.new "Could not determine format given the contents of #{@options[:output_path]}"
      end

      file_name = @options[:file_name] || formatter.default_file_name
      if @options[:create_folders]
        @strings.language_codes.each do |lang|
          output_path = File.join(@options[:output_path], formatter.output_path_for_language(lang))

          FileUtils.mkdir_p(output_path)

          file_path = File.join(output_path, file_name)

          output = formatter.format_file(lang)
          unless output
            Twine::stderr.puts "Skipping file at path #{file_path} since it would not contain any strings."
            next
          end

          IO.write(file_path, output, encoding: encoding)
        end
      else
        language_found = false
        Dir.foreach(@options[:output_path]) do |item|
          next if item == "." or item == ".."

          output_path = File.join(@options[:output_path], item)
          next unless File.directory?(output_path)

          lang = formatter.determine_language_given_path(output_path)
          next unless lang

          language_found = true

          file_path = File.join(output_path, file_name)
          output = formatter.format_file(lang)
          unless output
            Twine::stderr.puts "Skipping file at path #{file_path} since it would not contain any strings."
            next
          end

          IO.write(file_path, output, encoding: encoding)
        end

        unless language_found
          raise Twine::Error.new("Failed to generate any files: No languages found at #{@options[:output_path]}")
        end
      end

    end

    def consume_string_file
      lang = nil
      if @options[:languages]
        lang = @options[:languages][0]
      end

      read_string_file(@options[:input_path], lang)
      output_path = @options[:output_path] || @options[:strings_file]
      write_strings_data(output_path)
    end

    def consume_all_string_files
      if !File.directory?(@options[:input_path])
        raise Twine::Error.new("Directory does not exist: #{@options[:output_path]}")
      end

      Dir.glob(File.join(@options[:input_path], "**/*")) do |item|
        if File.file?(item)
          begin
            read_string_file(item)
          rescue Twine::Error => e
            Twine::stderr.puts "#{e.message}"
          end
        end
      end

      output_path = @options[:output_path] || @options[:strings_file]
      write_strings_data(output_path)
    end

    def generate_loc_drop
      validate_strings_file if @options[:validate]
      
      require_rubyzip

      if File.file?(@options[:output_path])
        File.delete(@options[:output_path])
      end

      Dir.mktmpdir do |temp_dir|
        Zip::File.open(@options[:output_path], Zip::File::CREATE) do |zipfile|
          zipfile.mkdir('Locales')

          formatter = formatter_for_format(@options[:format])
          @strings.language_codes.each do |lang|
            if @options[:languages] == nil || @options[:languages].length == 0 || @options[:languages].include?(lang)
              file_name = lang + formatter.extension
              temp_path = File.join(temp_dir, file_name)
              zip_path = File.join('Locales', file_name)

              output = formatter.format_file(lang)
              unless output
                Twine::stderr.puts "Skipping file #{file_name} since it would not contain any strings."
                next
              end
              
              IO.write(temp_path, output, encoding: encoding)
              zipfile.add(zip_path, temp_path)
            end
          end
        end
      end
    end

    def consume_loc_drop
      require_rubyzip

      if !File.file?(@options[:input_path])
        raise Twine::Error.new("File does not exist: #{@options[:input_path]}")
      end

      Dir.mktmpdir do |temp_dir|
        Zip::File.open(@options[:input_path]) do |zipfile|
          zipfile.each do |entry|
            next if entry.name.end_with? '/' or File.basename(entry.name).start_with? '.'

            real_path = File.join(temp_dir, entry.name)
            FileUtils.mkdir_p(File.dirname(real_path))
            zipfile.extract(entry.name, real_path)
            begin
              read_string_file(real_path)
            rescue Twine::Error => e
              Twine::stderr.puts "#{e.message}"
            end
          end
        end
      end

      output_path = @options[:output_path] || @options[:strings_file]
      write_strings_data(output_path)
    end

    def validate_strings_file
      total_strings = 0
      all_keys = Set.new
      duplicate_keys = Set.new
      keys_without_tags = Set.new
      invalid_keys = Set.new
      valid_key_regex = /^[A-Za-z0-9_]+$/

      @strings.sections.each do |section|
        section.rows.each do |row|
          total_strings += 1

          duplicate_keys.add(row.key) if all_keys.include? row.key
          all_keys.add(row.key)

          keys_without_tags.add(row.key) if row.tags == nil or row.tags.length == 0

          invalid_keys << row.key unless row.key =~ valid_key_regex
        end
      end

      errors = []
      join_keys = lambda { |set| set.map { |k| "  " + k }.join("\n") }

      unless duplicate_keys.empty?
        errors << "Found duplicate string key(s):\n#{join_keys.call(duplicate_keys)}"
      end

      if @options[:pedantic]
        if keys_without_tags.length == total_strings
          errors << "None of your strings have tags."
        elsif keys_without_tags.length > 0
          errors << "Found strings without tags:\n#{join_keys.call(keys_without_tags)}"
        end
      end

      unless invalid_keys.empty?
        errors << "Found key(s) with invalid characters:\n#{join_keys.call(invalid_keys)}"
      end

      raise Twine::Error.new errors.join("\n\n") unless errors.empty?

      Twine::stdout.puts "#{@options[:strings_file]} is valid."
    end

    private

    def encoding
      @options[:output_encoding] || 'UTF-8'
    end

    def require_rubyzip
      begin
        require 'zip'
      rescue LoadError
        raise Twine::Error.new "You must run 'gem install rubyzip' in order to create or consume localization drops."
      end
    end

    def determine_language_given_path(path)
      code = File.basename(path, File.extname(path))
      return code if @strings.language_codes.include? code
    end

    def formatter_for_format(format)
      find_formatter { |f| f.format_name == format }
    end

    def find_formatter(&block)
      formatter = Formatters.formatters.find &block
      return nil unless formatter
      formatter.strings = @strings
      formatter.options = @options
      formatter
    end

    def read_string_file(path, lang = nil)
      unless File.file?(path)
        raise Twine::Error.new("File does not exist: #{path}")
      end

      formatter, lang = prepare_read_write(path, lang)

      encoding = @options[:encoding] || Twine::Encoding.encoding_for_path(path)

      IO.open(IO.sysopen(path, 'rb'), 'rb', external_encoding: encoding, internal_encoding: 'UTF-8') do |io|
        io.read(2) if Twine::Encoding.has_bom?(path)
        formatter.read(io, lang)
      end
    end

    def prepare_read_write(path, lang)
      formatter_for_path = find_formatter { |f| f.extension == File.extname(path) }
      formatter = formatter_for_format(@options[:format]) || formatter_for_path
      
      unless formatter
        raise Twine::Error.new "Unable to determine format of #{path}"
      end      

      lang = lang || determine_language_given_path(path) || formatter.determine_language_given_path(path)
      unless lang
        raise Twine::Error.new "Unable to determine language for #{path}"
      end

      @strings.language_codes << lang unless @strings.language_codes.include? lang

      return formatter, lang
    end
  end
end
