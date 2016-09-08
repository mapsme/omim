module Twine
  module Formatters
    class Django < Abstract
      def format_name
        'django'
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
        comment_regex = /#\. *"?(.*)"?$/
        key_regex = /msgid *"(.*)"$/
        value_regex = /msgstr *"(.*)"$/m

        last_comment = nil
        while line = io.gets          
          comment_match = comment_regex.match(line)
          if comment_match
            comment = comment_match[1]
          end

          key_match = key_regex.match(line)
          if key_match
            key = key_match[1].gsub('\\"', '"')
          end
          value_match = value_regex.match(line)
          if value_match
            value = value_match[1].gsub(/"\n"/, '').gsub('\\"', '"')
          end

          if key and key.length > 0 and value and value.length > 0
            set_translation_for_key(key, lang, value)
            if comment and comment.length > 0 and !comment.start_with?("--------- ")
              set_comment_for_key(key, comment)
            end
            key = nil
            value = nil
            comment = nil
          end
        end
      end

      def format_file(lang)
        @default_lang = @strings.language_codes[0]
        result = super
        @default_lang = nil
        result
      end

      def format_header(lang)
        "##\n # Django Strings File\n # Generated by Twine #{Twine::VERSION}\n # Language: #{lang}\n"
      end

      def format_section_header(section)
        "#--------- #{section.name} ---------#\n"
      end

      def format_row(row, lang)
        [format_comment(row, lang), format_base_translation(row), format_key_value(row, lang)].compact.join
      end

      def format_base_translation(row)
        base_translation = row.translations[@default_lang]
        "# base translation: \"#{base_translation}\"\n" if base_translation
      end

      def key_value_pattern
        "msgid \"%{key}\"\n" +
        "msgstr \"%{value}\"\n"
      end

      def format_comment(row, lang)
        "#. #{escape_quotes(row.comment)}\n" if row.comment
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

Twine::Formatters.formatters << Twine::Formatters::Django.new
