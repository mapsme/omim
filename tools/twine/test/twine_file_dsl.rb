module TwineFileDSL
  def build_twine_file(*languages)
    @currently_built_twine_file = Twine::StringsFile.new
    @currently_built_twine_file.language_codes.concat languages
    yield
    result = @currently_built_twine_file
    @currently_built_twine_file = nil
    return result
  end

  def add_section(name)
    return unless @currently_built_twine_file
    @currently_built_twine_file_section = Twine::StringsSection.new name
    @currently_built_twine_file.sections << @currently_built_twine_file_section
    yield
    @currently_built_twine_file_section = nil
  end

  def add_row(parameters)
    return unless @currently_built_twine_file
    return unless @currently_built_twine_file_section

    # this relies on Ruby preserving the order of hash elements
    key, value = parameters.first
    row = Twine::StringsRow.new(key.to_s)
    if value.is_a? Hash
      value.each do |language, translation|
        row.translations[language.to_s] = translation
      end
    elsif !value.is_a? Symbol
      language = @currently_built_twine_file.language_codes.first
      row.translations[language] = value
    end

    row.comment = parameters[:comment] if parameters[:comment]
    row.tags = parameters[:tags] if parameters[:tags]
    if parameters[:ref] || value.is_a?(Symbol)
      reference_key = (parameters[:ref] || value).to_s
      row.reference_key = reference_key
      row.reference = @currently_built_twine_file.strings_map[reference_key]
    end

    @currently_built_twine_file_section.rows << row
    @currently_built_twine_file.strings_map[row.key] = row
  end
end
