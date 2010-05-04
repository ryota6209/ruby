module YAML
  class << self
    def engine
      ::Thread.current[:yamler] or
        (ENGINE.yamler = ENGINE.default; ::Thread.current[:yamler])
    end

    def const_defined?(name)
      engine.const_defined?(name)
    end

    def const_get(name)
      engine = self.engine
      begin
        engine.const_get(name)
      rescue NameError => e
        raise NameError, "uninitialized constant #{self}::#{name}", caller(2)
      end
    end

    alias const_missing const_get

    def method_missing(name, *args, &block)
      engine.__send__(name, *args, &block)
    end

    def respond_to_missing?(name)
      engine.respond_to?(name)
    end

  private

    def extend_object(obj)
      engine.__send__(:extend_object, obj)
    end

    def append_features(mod)
      engine.__send__(:append_features, mod)
    end
  end

  class << (ENGINE = Object.new) # :nodoc:
    def default
      !defined?(Syck) && defined?(Psych) ? 'psych' : 'syck'
    end

    def yamler
      if engine = YAML.engine
        engine.name.downcase
      else
        default
      end
    end

    def syck?
      'syck' == yamler
    end

    def yamler= engine
      raise(ArgumentError, "bad engine") unless %w{syck psych}.include?(engine)
      require engine
      engine = ::Object.const_get(engine.capitalize)
      ::Thread.current[:yamler] = engine
    end
  end

  ENGINE.freeze
end
