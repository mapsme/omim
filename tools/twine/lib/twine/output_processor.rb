module Twine
  module Processors

    class OutputProcessor
      def initialize(strings, options)
        @strings = strings
        @options = options
      end

      def default_language
        @options[:developer_language] || @strings.language_codes[0]
      end

      def fallback_languages(language)
        fallback_mapping = {
          'zh-TW' => 'zh-Hant' # if we don't have a zh-TW translation, try zh-Hant before en
        }

        [fallback_mapping[language], default_language].flatten.compact
      end

      def process(language)
        result = StringsFile.new

        result.language_codes.concat @strings.language_codes
        @strings.sections.each do |section|
          new_section = StringsSection.new section.name

          section.rows.each do |row|
            next unless row.matches_tags?(@options[:tags], @options[:untagged])

            value = row.translated_string_for_lang(language)

            next if value && @options[:include] == :untranslated

            if value.nil? && @options[:include] != :translated
              value = row.translated_string_for_lang(fallback_languages(language))
            end

            next unless value

            new_row = row.dup
            new_row.translations[language] = value

            new_section.rows << new_row
            result.strings_map[new_row.key] = new_row
          end

          result.sections << new_section
        end

        return result
      end
    end

  end
end
