# encoding: utf-8

module Twine
  module Formatters
    class Gettext < Abstract
      def format_name
        'gettext'
      end

      def extension
        '.po'
      end

      def can_handle_directory?(path)
        Dir.entries(path).any? { |item| /^.+\.po$/.match(item) }
      end

      def default_file_name
        return 'strings.po'
      end

      def determine_language_given_path(path)
        path_arr = path.split(File::SEPARATOR)
        path_arr.each do |segment|
          match = /(..)\.po$/.match(segment)
          if match
            return match[1]
          end
        end

        return
      end

      def read(io, lang)
        comment_regex = /#.? *"(.*)"$/
        key_regex = /msgctxt *"(.*)"$/
        value_regex = /msgstr *"(.*)"$/m
        
        while item = io.gets("\n\n")
          key = nil
          value = nil
          comment = nil

          comment_match = comment_regex.match(item)
          if comment_match
            comment = comment_match[1]
          end
          key_match = key_regex.match(item)
          if key_match
            key = key_match[1].gsub('\\"', '"')
          end
          value_match = value_regex.match(item)
          if value_match
            value = value_match[1].gsub(/"\n"/, '').gsub('\\"', '"')
          end
          if key and key.length > 0 and value and value.length > 0
            set_translation_for_key(key, lang, value)
            if comment and comment.length > 0 and !comment.start_with?("SECTION:")
              set_comment_for_key(key, comment)
            end
            comment = nil
          end
        end
      end

      def format_file(lang)
        @default_lang = strings.language_codes[0]
        result = super
        @default_lang = nil
        result
      end

      def format_header(lang)
        "msgid \"\"\nmsgstr \"\"\n\"Language: #{lang}\\n\"\n\"X-Generator: Twine #{Twine::VERSION}\\n\"\n"
      end

      def format_section_header(section)
        "# SECTION: #{section.name}"
      end

      def should_include_row(row, lang)
        super and row.translated_string_for_lang(@default_lang)
      end

      def format_comment(row, lang)
        "#. \"#{escape_quotes(row.comment)}\"\n" if row.comment
      end

      def format_key_value(row, lang)
        value = row.translated_string_for_lang(lang)
        [format_key(row.key.dup), format_base_translation(row), format_value(value.dup)].compact.join
      end

      def format_key(key)
        "msgctxt \"#{key}\"\n"
      end

      def format_base_translation(row)
        "msgid \"#{row.translations[@default_lang]}\"\n"
      end

      def format_value(value)
        "msgstr \"#{value}\"\n"
      end
    end
  end
end

Twine::Formatters.formatters << Twine::Formatters::Gettext.new
