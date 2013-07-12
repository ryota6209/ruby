class RbInstall
  module Specs
    class Reader < Struct.new(:src, :extout, :arch)
      def gemspec
        @gemspec ||= begin
          spec = Gem::Specification.load(src) || raise("invalid spec in #{src}")
          file_collector = FileCollector.new(File.dirname(src), extout, arch)
          spec.files = file_collector.collect
          spec
        end
      end

      def spec_source
        @gemspec.to_ruby
      end
    end
  end
end
