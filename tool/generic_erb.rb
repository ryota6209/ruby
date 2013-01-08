# -*- coding: us-ascii -*-
require 'erb'
require 'optparse'
require 'fileutils'
$:.unshift(File.dirname(__FILE__))
require 'vpath'

vpath = VPath.new
timestamp = nil
output = nil
ifchange = nil
dump = nil

opt = OptionParser.new do |o|
  o.on('-t', '--timestamp[=PATH]') {|v| timestamp = v || true}
  o.on('-o', '--output=PATH') {|v| output = v}
  o.on('-c', '--[no-]if-change') {|v| ifchange = v}
  o.on('-n', '--show-source') {dump = 'src'}
  o.on('--dump=TYPE', %w[insn src]) {|v| dump = v}
  vpath.def_options(o)
  o.order!(ARGV)
end
template = ARGV.shift or abort opt.to_s
erb = ERB.new(File.read(template), nil, '%')
erb.filename = template
case dump
when 'src'
  puts erb.src
  exit
when 'insn'
  puts RubyVM::InstructionSequence.compile(erb.src, erb.filename, nil, 0).disasm
  exit
end
result = erb.result
if output
  if ifchange and (vpath.open(output) {|f| f.read} rescue nil) == result
    puts "#{output} unchanged"
  else
    open(output, "wb") {|f| f.print result}
    puts "#{output} updated"
  end
  if timestamp
    if timestamp == true
      dir, base = File.split(output)
      timestamp = File.join(dir, ".time." + base)
    end
    FileUtils.touch(timestamp)
  end
else
  print result
end
