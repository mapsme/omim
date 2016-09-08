require 'optparse'

module Twine
  module CLI
    NEEDED_COMMAND_ARGUMENTS = {
      'generate-string-file' => 3,
      'generate-all-string-files' => 3,
      'consume-string-file' => 3,
      'consume-all-string-files' => 3,
      'generate-loc-drop' => 3,
      'consume-loc-drop' => 3,
      'validate-strings-file' => 2
    }

    def self.parse(args)
      options = { include: :all }

      parser = OptionParser.new do |opts|
        opts.banner = 'Usage: twine COMMAND STRINGS_FILE [INPUT_OR_OUTPUT_PATH] [--lang LANG1,LANG2...] [--tags TAG1,TAG2,TAG3...] [--format FORMAT]'
        opts.separator ''
        opts.separator 'The purpose of this script is to convert back and forth between multiple data formats, allowing us to treat our strings (and translations) as data stored in a text file. We can then use the data file to create drops for the localization team, consume similar drops returned by the localization team, and create formatted string files to ship with your products.'
        opts.separator ''
        opts.separator 'Commands:'
        opts.separator ''
        opts.separator '- generate-string-file'
        opts.separator '    Generates a string file in a certain LANGUAGE given a particular FORMAT. This script will attempt to guess both the language and the format given the filename and extension. For example, "ko.xml" will generate a Korean language file for Android.'
        opts.separator ''
        opts.separator '- generate-all-string-files'
        opts.separator '    Generates all the string files necessary for a given project. The parent directory to all of the locale-specific directories in your project should be specified as the INPUT_OR_OUTPUT_PATH. This command will most often be executed by your build script so that each build always contains the most recent strings.'
        opts.separator ''
        opts.separator '- consume-string-file'
        opts.separator '    Slurps all of the strings from a translated strings file into the specified STRINGS_FILE. If you have some files returned to you by your translators you can use this command to incorporate all of their changes. This script will attempt to guess both the language and the format given the filename and extension. For example, "ja.strings" will assume that the file is a Japanese iOS strings file.'
        opts.separator ''
        opts.separator '- consume-all-string-files'
        opts.separator '    Slurps all of the strings from a directory into the specified STRINGS_FILE. If you have some files returned to you by your translators you can use this command to incorporate all of their changes. This script will attempt to guess both the language and the format given the filename and extension. For example, "ja.strings" will assume that the file is a Japanese iOS strings file.'
        opts.separator ''
        opts.separator '- generate-loc-drop'
        opts.separator '    Generates a zip archive of strings files in any format. The purpose of this command is to create a very simple archive that can be handed off to a translation team. The translation team can unzip the archive, translate all of the strings in the archived files, zip everything back up, and then hand that final archive back to be consumed by the consume-loc-drop command.'
        opts.separator ''
        opts.separator '- consume-loc-drop'
        opts.separator '    Consumes an archive of translated files. This archive should be in the same format as the one created by the generate-loc-drop command.'
        opts.separator ''
        opts.separator '- validate-strings-file'
        opts.separator '    Validates that the given strings file is parseable, contains no duplicates, and that every string has a tag. Exits with a non-zero exit code if those criteria are not met.'
        opts.separator ''
        opts.separator 'General Options:'
        opts.separator ''
        opts.on('-l', '--lang LANGUAGES', Array, 'The language code(s) to use for the specified action.') do |l|
          options[:languages] = l
        end
        opts.on('-t', '--tags TAG1,TAG2,TAG3', Array, 'The tag(s) to use for the specified action. Only strings with that tag will be processed. Omit this option to match',
                                                      '  all strings in the strings data file.') do |t|
          options[:tags] = t
        end
        opts.on('-u', '--[no-]untagged', 'If you have specified tags using the --tags flag, then only those tags will be selected. If you also want to select',
                                         '  all strings that are untagged, then you can specify this option to do so.') do |u|
          options[:untagged] = u
        end
        formats = Formatters.formatters.map(&:format_name).map(&:downcase)
        opts.on('-f', '--format FORMAT', formats, "The file format to read or write: (#{formats.join(', ')}).",
                                                  "  Additional formatters can be placed in the formats/ directory.") do |f|
          options[:format] = f
        end
        opts.on('-a', '--[no-]consume-all', 'Normally, when consuming a string file, Twine will ignore any string keys that do not exist in your master file.') do |a|
          options[:consume_all] = true
        end
        opts.on('-i', '--include SET', [:all, :translated, :untranslated],
                                        "This flag will determine which strings are included when generating strings files. It's possible values:",
                                        "  all: All strings both translated and untranslated for the specified language are included. This is the default value.",
                                        "  translated: Only translated strings are included.",
                                        "  untranslated: Only untranslated strings are included.") do |i|
          options[:include] = i
        end
        opts.on('-o', '--output-file OUTPUT_FILE', 'Write the new strings database to this file instead of replacing the original file. This flag is only useful when',
                                                   '  running the consume-string-file or consume-loc-drop commands.') do |o|
          options[:output_path] = o
        end
        opts.on('-n', '--file-name FILE_NAME', 'When running the generate-all-string-files command, this flag may be used to overwrite the default file name of the format.') do |n|
          options[:file_name] = n
        end
        opts.on('-r', '--[no-]create-folders', "When running the generate-all-string-files command, this flag may be used to create output folders for all languages,",
                                               "  if they  don't exist yet. As a result all languages will be exported, not only the ones where an output folder already",
                                               "  exists.") do |r|
          options[:create_folders] = r
        end
        opts.on('-d', '--developer-language LANG', 'When writing the strings data file, set the specified language as the "developer language". In practice, this just',
                                                   '  means that this language will appear first in the strings data file. When generating files this language will be',
                                                   '  used as default language and its translations will be used if a key is not localized for the output language.') do |d|
          options[:developer_language] = d
        end
        opts.on('-c', '--[no-]consume-comments', 'Normally, when consuming a string file, Twine will ignore all comments in the file. With this flag set, any comments',
                                                 '  encountered will be read and parsed into the strings data file. This is especially useful when creating your first',
                                                 '  strings data file from an existing project.') do |c|
          options[:consume_comments] = c
        end
        opts.on('-e', '--encoding ENCODING', 'Twine defaults to encoding all output files in UTF-8. This flag will tell Twine to use an alternate encoding for these',
                                             '  files. For example, you could use this to write Apple .strings files in UTF-16. When reading files, Twine does its best',
                                             "  to determine the encoding automatically. However, if the files are UTF-16 without BOM, you need to specify if it's", 
                                             '  UTF-16LE or UTF16-BE.') do |e|
          options[:output_encoding] = e
        end
        opts.on('--[no-]validate', 'Validate the strings file before formatting it.') do |validate|
          options[:validate] = validate
        end
        opts.on('-p', '--[no-]pedantic', 'When validating a strings file, perform additional checks that go beyond pure validity (like presence of tags).') do |p|
          options[:pedantic] = p
        end
        opts.on('-h', '--help', 'Show this message.') do |h|
          puts opts.help
          exit
        end
        opts.on('--version', 'Print the version number and exit.') do
          puts "Twine version #{Twine::VERSION}"
          exit
        end
        opts.separator ''
        opts.separator 'Examples:'
        opts.separator ''
        opts.separator '> twine generate-string-file strings.txt ko.xml --tags FT'
        opts.separator '> twine generate-all-string-files strings.txt Resources/Locales/ --tags FT,FB'
        opts.separator '> twine consume-string-file strings.txt ja.strings'
        opts.separator '> twine consume-all-string-files strings.txt Resources/Locales/ --developer-language en --tags DefaultTag1,DefaultTag2'
        opts.separator '> twine generate-loc-drop strings.txt LocDrop5.zip --tags FT,FB --format android --lang de,en,en-GB,ja,ko'
        opts.separator '> twine consume-loc-drop strings.txt LocDrop5.zip'
        opts.separator '> twine validate-strings-file strings.txt'
      end
      begin
        parser.parse! args
      rescue OptionParser::ParseError => e
        Twine::stderr.puts e.message
        exit false
      end

      if args.length == 0
        puts parser.help
        exit false
      end

      number_of_needed_arguments = NEEDED_COMMAND_ARGUMENTS[args[0]]
      unless number_of_needed_arguments
        raise Twine::Error.new "Invalid command: #{args[0]}"
      end
      options[:command] = args[0]

      if args.length < 2
        raise Twine::Error.new 'You must specify your strings file.'
      end
      options[:strings_file] = args[1]

      if args.length < number_of_needed_arguments
        raise Twine::Error.new 'Not enough arguments.'
      elsif args.length > number_of_needed_arguments
        raise Twine::Error.new "Unknown argument: #{args[number_of_needed_arguments]}"
      end

      case options[:command]
      when 'generate-string-file'
        options[:output_path] = args[2]
        if options[:languages] and options[:languages].length > 1
          raise Twine::Error.new 'Please only specify a single language for the generate-string-file command.'
        end
      when 'generate-all-string-files'
        options[:output_path] = args[2]
      when 'consume-string-file'
        options[:input_path] = args[2]
        if options[:languages] and options[:languages].length > 1
          raise Twine::Error.new 'Please only specify a single language for the consume-string-file command.'
        end
      when 'consume-all-string-files'
        options[:input_path] = args[2]
      when 'generate-loc-drop'
        options[:output_path] = args[2]
        if !options[:format]
          raise Twine::Error.new 'You must specify a format.'
        end
      when 'consume-loc-drop'
        options[:input_path] = args[2]
      when 'validate-strings-file'
      end

      return options
    end
  end
end
