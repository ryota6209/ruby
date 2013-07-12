class RbInstall
  module Specs
    class Generator < Struct.new(:name, :base_dir, :src, :execs, :extout, :arch)
      def gemspec
        @gemspec ||= eval spec_source
      end

      def spec_source
        <<-GEMSPEC
Gem::Specification.new do |s|
  s.name = #{name.dump}
  s.version = #{version.dump}
  s.summary = "This #{name} is bundled with Ruby"
  s.executables = #{execs.inspect}
  s.files = #{files.inspect}
end
        GEMSPEC
      end

      private
      def version
        version = open(src) { |f|
          f.find { |s| /^\s*\w*VERSION\s*=(?!=)/ =~ s }
        } or return
        version.split(%r"=\s*", 2)[1].strip[/\A([\'\"])(.*?)\1/, 2]
      end

      def files
        file_collector = FileCollector.new(base_dir, extout, arch)
        file_collector.collect
      end
    end
  end
end
