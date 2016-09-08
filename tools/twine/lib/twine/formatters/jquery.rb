module Twine
  module Formatters
    class JQuery < Abstract
      def format_name
        'jquery'
      end

      def extension
        '.json'
      end

      def can_handle_directory?(path)
        Dir.entries(path).any? { |item| /^.+\.json$/.match(item) }
      end

      def default_file_name
        return 'localize.json'
      end

      def determine_language_given_path(path)
        path_arr = path.split(File::SEPARATOR)
        path_arr.each do |segment|
          match = /^((.+)-)?([^-]+)\.json$/.match(segment)
          if match
            return match[3]
          end
        end

        return
      end

      def read(io, lang)
        begin
          require "json"
        rescue LoadError
          raise Twine::Error.new "You must run 'gem install json' in order to read or write jquery-localize files."
        end

        json = JSON.load(io)
        json.each do |key, value|
          set_translation_for_key(key, lang, value)
        end
      end

      def format_file(lang)
        result = super
        return result unless result
        "{\n#{super}\n}\n"
      end

      def format_sections(strings, lang)
        sections = strings.sections.map { |section| format_section(section, lang) }
        sections.join(",\n\n")
      end

      def format_section_header(section)
      end

      def format_section(section, lang)
        rows = section.rows.dup

        rows.map! { |row| format_row(row, lang) }
        rows.compact! # remove nil entries
        rows.join(",\n")
      end

      def key_value_pattern
        "\"%{key}\":\"%{value}\""
      end

      def format_key(key)
        escape_quotes(key)
      end

      def format_value(value)
        escape_quotes(value)
      end
    end
  end
end

Twine::Formatters.formatters << Twine::Formatters::JQuery.new
