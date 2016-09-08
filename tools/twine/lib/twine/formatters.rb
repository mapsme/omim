module Twine
  module Formatters
    @formatters = []

    class << self
      attr_reader :formatters

      ###
      # registers a new formatter
      #
      # formatter_class - the class of the formatter to register
      #
      # returns array of active formatters
      #
      def register_formatter formatter_class
        @formatters << formatter_class.new
      end
    end
  end
end

require File.join(File.dirname(__FILE__), 'formatters', 'abstract.rb')
Dir[File.join(File.dirname(__FILE__), 'formatters', '*.rb')].each do |file|
  require file
end
