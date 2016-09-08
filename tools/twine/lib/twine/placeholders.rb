module Twine
  module Placeholders
    extend self

    PLACEHOLDER_FLAGS_WIDTH_PRECISION_LENGTH = '([-+ 0#])?(\d+|\*)?(\.(\d+|\*))?(hh?|ll?|L|z|j|t)?'
    PLACEHOLDER_PARAMETER_FLAGS_WIDTH_PRECISION_LENGTH = '(\d+\$)?' + PLACEHOLDER_FLAGS_WIDTH_PRECISION_LENGTH

    # http://developer.android.com/guide/topics/resources/string-resource.html#FormattingAndStyling
    # http://stackoverflow.com/questions/4414389/android-xml-percent-symbol
    # https://github.com/mobiata/twine/pull/106
    def convert_placeholders_from_twine_to_android(input)
      placeholder_types = '[diufFeEgGxXoscpaA]'

      # %@ -> %s
      value = input.gsub(/(%#{PLACEHOLDER_PARAMETER_FLAGS_WIDTH_PRECISION_LENGTH})@/, '\1s')

      placeholder_syntax = PLACEHOLDER_PARAMETER_FLAGS_WIDTH_PRECISION_LENGTH + placeholder_types
      placeholder_regex = /%#{placeholder_syntax}/

      number_of_placeholders = value.scan(placeholder_regex).size

      return value if number_of_placeholders == 0

      # got placeholders -> need to double single percent signs
      # % -> %% (but %% -> %%, %d -> %d)
      single_percent_regex = /([^%])(%)(?!(%|#{placeholder_syntax}))/
      value.gsub! single_percent_regex, '\1%%'

      return value if number_of_placeholders < 2

      # number placeholders
      non_numbered_placeholder_regex = /%(#{PLACEHOLDER_FLAGS_WIDTH_PRECISION_LENGTH}#{placeholder_types})/

      number_of_non_numbered_placeholders = value.scan(non_numbered_placeholder_regex).size

      return value if number_of_non_numbered_placeholders == 0

      raise Twine::Error.new("The value \"#{input}\" contains numbered and non-numbered placeholders") if number_of_placeholders != number_of_non_numbered_placeholders

      # %d -> %$1d
      index = 0
      value.gsub!(non_numbered_placeholder_regex) { "%#{index += 1}$#{$1}" }

      value
    end

    def convert_placeholders_from_android_to_twine(input)
      placeholder_regex = /(%#{PLACEHOLDER_PARAMETER_FLAGS_WIDTH_PRECISION_LENGTH})s/

      # %s -> %@
      input.gsub(placeholder_regex, '\1@')
    end
  end
end
