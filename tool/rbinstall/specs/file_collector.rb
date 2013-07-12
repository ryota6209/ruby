class RbInstall
  module Specs
    class FileCollector
      def initialize(base_dir, extout, arch)
        @base_dir = base_dir
        @extout = extout
        @arch = arch
      end

      def collect
        ruby_libraries + built_libraries
      end

      private
      def type
        @base_dir[/\/(ext|lib)?\/.*?\z/, 1]
      end

      def ruby_libraries
        case type
        when "ext"
          prefix = "#{@extout}/common/"
          base = "#{prefix}#{relative_base}"
        when "lib"
          base = @base_dir
          prefix = base.sub(/lib\/.*?\z/, "") + "lib/"
        end

        Dir.glob("#{base}{.rb,/**/*.rb}").collect do |ruby_source|
          remove_prefix(prefix, ruby_source)
        end
      end

      def built_libraries
        case type
        when "ext"
          prefix = "#{@extout}/#{@arch}/"
          base = "#{prefix}#{relative_base}"
          Dir.glob("#{base}{.so,/**/*.so}").collect do |built_library|
            remove_prefix(prefix, built_library)
          end
        when "lib"
          []
        end
      end

      def relative_base
        @base_dir[/\/#{Regexp.escape(type)}\/(.*?)\z/, 1]
      end

      def remove_prefix(prefix, string)
        string.sub(/\A#{Regexp.escape(prefix)}/, "")
      end
    end
  end
end
