module Gem
  if defined?(Specification)
    remove_const(:Specification)
  end

  class Specification < OpenStruct
    def initialize(*)
      super
      yield(self) if defined?(yield)
      self.executables ||= []
    end

    def self.load(path)
      src = File.open(path, "rb") {|f| f.read}
      src.sub!(/\A#.*/, '')
      eval(src, nil, path)
    end

    def to_ruby
      <<-GEMSPEC
Gem::Specification.new do |s|
  s.name = #{name.dump}
  s.version = #{version.dump}
  s.summary = #{summary.dump}
  s.description = #{description.dump}
  s.homepage = #{homepage.dump}
  s.authors = #{authors.inspect}
  s.email = #{email.inspect}
  s.files = #{files.inspect}
end
      GEMSPEC
    end
  end
end
