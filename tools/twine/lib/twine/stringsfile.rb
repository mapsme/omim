module Twine
  class StringsSection
    attr_reader :name
    attr_reader :rows

    def initialize(name)
      @name = name
      @rows = []
    end
  end

  class StringsRow
    attr_reader :key
    attr_accessor :comment
    attr_accessor :tags
    attr_reader :translations
    attr_accessor :reference
    attr_accessor :reference_key

    def initialize(key)
      @key = key
      @comment = nil
      @tags = nil
      @translations = {}
    end

    def comment
      raw_comment || (reference.comment if reference)
    end

    def raw_comment
      @comment
    end

    def matches_tags?(tags, include_untagged)
      if tags == nil || tags.empty?
        # The user did not specify any tags. Everything passes.
        return true
      elsif @tags == nil
        # This row has no tags.
        return reference ? reference.matches_tags?(tags, include_untagged) : include_untagged
      elsif @tags.empty?
        return include_untagged
      else
        return !(tags & @tags).empty?
      end

      return false
    end

    def translated_string_for_lang(lang)
      translation = [lang].flatten.map { |l| @translations[l] }.first

      translation = reference.translated_string_for_lang(lang) if translation.nil? && reference

      return translation
    end
  end

  class StringsFile
    attr_reader :sections
    attr_reader :strings_map
    attr_reader :language_codes

    private

    def match_key(text)
      match = /^\[(.+)\]$/.match(text)
      return match[1] if match
    end

    public

    def initialize
      @sections = []
      @strings_map = {}
      @language_codes = []
    end

    def add_language_code(code)
      if @language_codes.length == 0
        @language_codes << code
      elsif !@language_codes.include?(code)
        dev_lang = @language_codes[0]
        @language_codes << code
        @language_codes.delete(dev_lang)
        @language_codes.sort!
        @language_codes.insert(0, dev_lang)
      end
    end

    def set_developer_language_code(code)
      @language_codes.delete(code)
      @language_codes.insert(0, code)
    end

    def read(path)
      if !File.file?(path)
        raise Twine::Error.new("File does not exist: #{path}")
      end

      File.open(path, 'r:UTF-8') do |f|
        line_num = 0
        current_section = nil
        current_row = nil
        while line = f.gets
          parsed = false
          line.strip!
          line_num += 1

          if line.length == 0
            next
          end

          if line.length > 4 && line[0, 2] == '[['
            match = /^\[\[(.+)\]\]$/.match(line)
            if match
              current_section = StringsSection.new(match[1])
              @sections << current_section
              parsed = true
            end
          elsif line.length > 2 && line[0, 1] == '['
            key = match_key(line)
            if key
              current_row = StringsRow.new(key)
              @strings_map[current_row.key] = current_row
              if !current_section
                current_section = StringsSection.new('')
                @sections << current_section
              end
              current_section.rows << current_row
              parsed = true
            end
          else
            match = /^([^=]+)=(.*)$/.match(line)
            if match
              key = match[1].strip
              value = match[2].strip
              
              value = value[1..-2] if value[0] == '`' && value[-1] == '`'

              case key
              when 'comment'
                current_row.comment = value
              when 'tags'
                current_row.tags = value.split(',')
              when 'ref'
                current_row.reference_key = value if value
              else
                if !@language_codes.include? key
                  add_language_code(key)
                end
                current_row.translations[key] = value
              end
              parsed = true
            end
          end

          if !parsed
            raise Twine::Error.new("Unable to parse line #{line_num} of #{path}: #{line}")
          end
        end
      end

      # resolve_references
      @strings_map.each do |key, row|
        next unless row.reference_key
        row.reference = @strings_map[row.reference_key]
      end
    end

    def write(path)
      dev_lang = @language_codes[0]

      File.open(path, 'w:UTF-8') do |f|
        @sections.each do |section|
          if f.pos > 0
            f.puts ''
          end

          f.puts "[[#{section.name}]]"

          section.rows.each do |row|
            f.puts "\t[#{row.key}]"

            value = write_value(row, dev_lang, f)
            if !value && !row.reference_key
              puts "Warning: #{row.key} does not exist in developer language '#{dev_lang}'"
            end
            
            if row.reference_key
              f.puts "\t\tref = #{row.reference_key}"
            end
            if row.tags && row.tags.length > 0
              tag_str = row.tags.join(',')
              f.puts "\t\ttags = #{tag_str}"
            end
            if row.raw_comment and row.raw_comment.length > 0
              f.puts "\t\tcomment = #{row.raw_comment}"
            end
            @language_codes[1..-1].each do |lang|
              write_value(row, lang, f)
            end
          end
        end
      end
    end

    private

    def write_value(row, language, file)
      value = row.translations[language]
      return nil unless value

      if value[0] == ' ' || value[-1] == ' ' || (value[0] == '`' && value[-1] == '`')
        value = '`' + value + '`'
      end

      file.puts "\t\t#{language} = #{value}"
      return value
    end

  end
end
