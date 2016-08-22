#!/usr/bin/ruby
class DSL
  ISeq = RubyVM::InstructionSequence

  class << self
    def define_operator(op, method)
      (@userops ||= {})[op] = method
    end
    def eval(src)
      new.instance_eval(&ISeq.compile("proc {#{src}}", nil, nil, nil,
                                      userops: @userops).eval)
    end
  end
end

class MyDSL < DSL
  define_operator("<~~", "u2")
  define_operator("<~", "u1")

  def u1(x)
    [:u1, x]
  end
  def u2(x)
    [:u2, x]
  end
end
p MyDSL.eval("self <~~ 1")
