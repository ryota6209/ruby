$:.map! {|path| File.realpath(path)}
require 'fileutils'
require 'shellwords'
require 'optparse'
require 'optparse/shellwords'
require 'ostruct'

require_relative 'gem/specification'
require_relative 'specs/file_collector'
require_relative 'specs/reader'
require_relative 'specs/generator'
require_relative 'installers'

class RbInstall
  include FileUtils

  def self.run
    boot
    new.run
  end

  def self.boot
    begin
      load "./rbconfig.rb"
    rescue LoadError
      const_set(:CONFIG, Hash.new {""})
    else
      include RbConfig
      $".unshift File.expand_path("./rbconfig.rb")
    end

    def CONFIG.[](name, mandatory = false)
      value = super(name)
      if mandatory
        raise "CONFIG['#{name}'] must be set" if !value or value.empty?
      end
      value
    end

    @@srcdir = File.expand_path('../../..', __FILE__)
    unless defined?(CROSS_COMPILING) and CROSS_COMPILING
      $:.replace([@@srcdir+"/lib", Dir.pwd])
    end

    STDOUT.sync = true
    File.umask(0)

    self
  end

  INSTALL_PROCS = Hash.new {[]}

  def install?(*types, &block)
    INSTALL_PROCS[:all] <<= block
    types.each do |type|
      INSTALL_PROCS[type] <<= block
    end
  end

  def setup_installers
    install?(:local, :arch, :bin, :'bin-arch', &method(:install_binary_commands))
    install?(:local, :arch, :lib, &method(:install_base_libraries))
    install?(:local, :arch, :data, &method(:install_data))
    install?(:ext, :arch, :'ext-arch', &method(:install_arch_extension_objects))
    install?(:ext, :arch, :'ext-arch', &method(:install_arch_extension_headers))
    install?(:ext, :comm, :'ext-comm', &method(:install_comm_extension_scripts))
    install?(:ext, :comm, :'ext-comm', &method(:install_comm_extension_headers))
    install?(:doc, :rdoc, &method(:install_rdoc))
    install?(:doc, :capi, &method(:install_capi_docs))
    install?(:local, :comm, :bin, :'bin-comm', &method(:install_command_scripts))
    install?(:local, :comm, :lib, &method(:install_library_scripts))
    install?(:local, :arch, :lib, &method(:install_common_headers))
    install?(:local, :comm, :man, &method(:install_manpages))
    install?(:ext, :comm, :gem, &method(:install_default_gems))
  end

  def initialize
    @made_dirs = {}

    @exeext = CONFIG["EXEEXT"]

    @ruby_install_name = CONFIG["ruby_install_name", true]
    @rubyw_install_name = CONFIG["rubyw_install_name"]
    @goruby_install_name = "go" + @ruby_install_name

    @arch = CONFIG["arch", true]
    @bindir = CONFIG["bindir", true]
    @libdir = CONFIG[CONFIG.fetch("libdirname", "libdir"), true]
    @rubyhdrdir = CONFIG["rubyhdrdir", true]
    @archhdrdir = CONFIG["rubyarchhdrdir"] || (@rubyhdrdir + "/" + @arch)
    @rubylibdir = CONFIG["rubylibdir", true]
    @archlibdir = CONFIG["rubyarchdir", true]
    @sitelibdir = CONFIG["sitelibdir"]
    @sitearchlibdir = CONFIG["sitearchdir"]
    @vendorlibdir = CONFIG["vendorlibdir"]
    @vendorarchlibdir = CONFIG["vendorarchdir"]
    @mandir = CONFIG["mandir", true]
    @docdir = CONFIG["docdir", true]
    @configure_args = Shellwords.shellwords(CONFIG["configure_args"])
    @enable_shared = CONFIG["ENABLE_SHARED"] == 'yes'
    @dll = CONFIG["LIBRUBY_SO", @enable_shared]
    @lib = CONFIG["LIBRUBY", true]
    @arc = CONFIG["LIBRUBY_A", true]
    @major = CONFIG["MAJOR", true]
    @minor = CONFIG["MINOR", true]
    @load_relative = @configure_args.include?("--enable-load-relative")

    if @load_relative
      @prolog_script = <<EOS
#!/bin/sh\n# -*- ruby -*-
bindir=`#{CONFIG["CHDIR"]} "${0%/*}" 2>/dev/null; pwd`
EOS

      if CONFIG["LIBRUBY_RELATIVE"] != 'yes' and libpathenv = CONFIG["LIBPATHENV"]
        pathsep = File::PATH_SEPARATOR
        @prolog_script << <<EOS
prefix="${bindir%/bin}"
export #{libpathenv}="$prefix/lib${#{libpathenv}#{pathsep}+#{pathsep}$#{libpathenv}}"
EOS
      end
      @prolog_script << %Q[exec "$bindir/#{@ruby_install_name}" -x "$0" "$@"\n]
    else
      @prolog_script = nil
    end

    setup_installers
  end

  def run
    parse_args

    extend FileUtils::NoWrite if @dryrun

    @fileutils_output = STDOUT
    @fileutils_label = ''

    @install << :local << :ext if @install.empty?
    @install.each do |inst|
      if !(procs = INSTALL_PROCS[inst]) || procs.empty?
        next warn("unknown install target - #{inst}")
      end
      procs.each do |block|
        dir = Dir.pwd
        begin
          block.call
        ensure
          Dir.chdir(dir)
        end
      end
    end
  end

  def parse_args(argv = ARGV)
    @mantype = 'doc'
    @destdir = nil
    @extout = nil
    @make = 'make'
    @mflags = []
    @install = []
    @installed_list = nil
    @dryrun = false
    @rdocdir = nil
    @data_mode = 0644
    @prog_mode = 0755
    @dir_mode = nil
    @script_mode = nil
    @strip = false
    @cmdtype = (if File::ALT_SEPARATOR == '\\'
        File.exist?("rubystub.exe") ? 'exe' : 'bat'
      end)
    mflags = []
    opt = OptionParser.new
    opt.on('-n', '--dry-run') {@dryrun = true}
    opt.on('--dest-dir=DIR') {|dir| @destdir = dir}
    opt.on('--extout=DIR') {|dir| @extout = (dir unless dir.empty?)}
    opt.on('--make=COMMAND') {|make| @make = make}
    opt.on('--mantype=MAN') {|man| @mantype = man}
    opt.on('--make-flags=FLAGS', '--mflags', Shellwords) do |v|
      if arg = v.first
        arg.insert(0, '-') if /\A[^-][^=]*\Z/ =~ arg
      end
      @mflags.concat(v)
    end
    opt.on('-i', '--install=TYPE', INSTALL_PROCS.keys) do |ins|
      @install << ins
    end
    opt.on('--data-mode=OCTAL-MODE', OptionParser::OctalInteger) do |mode|
      @data_mode = mode
    end
    opt.on('--prog-mode=OCTAL-MODE', OptionParser::OctalInteger) do |mode|
      @prog_mode = mode
    end
    opt.on('--dir-mode=OCTAL-MODE', OptionParser::OctalInteger) do |mode|
      @dir_mode = mode
    end
    opt.on('--script-mode=OCTAL-MODE', OptionParser::OctalInteger) do |mode|
      @script_mode = mode
    end
    opt.on('--installed-list [FILENAME]') {|name| @installed_list = name}
    opt.on('--rdoc-output [DIR]') {|dir| @rdocdir = dir}
    opt.on('--cmd-type=TYPE', %w[bat cmd plain]) {|cmd| @cmdtype = (cmd unless cmd == 'plain')}
    opt.on('--[no-]strip') {|strip| @strip = strip}

    opt.order!(argv) do |v|
      case v
      when /\AINSTALL[-_]([-\w]+)=(.*)/
        argv.unshift("--#{$1.tr('_', '-')}=#{$2}")
      when /\A\w[-\w+]*=\z/
        mflags << v
      when /\A\w[-\w+]*\z/
        @install << v.intern
      else
        raise OptionParser::InvalidArgument, v
      end
    end rescue abort "#{$!.message}\n#{opt.help}"

    unless defined?(RbConfig)
      puts opt.help
      exit
    end

    @make, *rest = Shellwords.shellwords(@make)
    @mflags.unshift(*rest) unless rest.empty?
    @mflags.unshift(*mflags)

    def @mflags.set?(flag)
      grep(/\A-(?!-).*#{flag.chr}/i) { return true }
      false
    end
    def @mflags.defined?(var)
      grep(/\A#{var}=(.*)/) {return block_given? ? yield($1) : $1}
      false
    end

    if @mflags.set?(?n)
      @dryrun = true
    else
      @mflags << '-n' if @dryrun
    end

    @destdir ||= @mflags.defined?("DESTDIR")
    if @extout ||= @mflags.defined?("EXTOUT")
      RbConfig.expand(@extout)
    end

    @continue = @mflags.set?(?k)

    if @installed_list ||= @mflags.defined?('INSTALLED_LIST')
      RbConfig.expand(@installed_list, RbConfig::CONFIG)
      @installed_list = open(@installed_list, "ab")
      @installed_list.sync = true
    end

    @rdocdir ||= @mflags.defined?('RDOCOUT')

    @dir_mode ||= @prog_mode | 0700
    @script_mode ||= @prog_mode
  end

  def strip_file(files)
    if !defined?(@strip_command) and (cmd = CONFIG["STRIP"])
      case cmd
      when "", "true", ":" then return
      else @strip_command = Shellwords.shellwords(cmd)
      end
    elsif !@strip_command
      return
    end
    system(*(@strip_command + [files].flatten))
  end

  def install(src, dest, options = {})
    options = options.clone
    strip = options.delete(:strip)
    options[:preserve] = true
    d = with_destdir(dest)
    super(src, d, options)
    srcs = Array(src)
    if strip
      d = srcs.map {|src| File.join(d, File.basename(src))} if @made_dirs[dest]
      strip_file(d)
    end
    if @installed_list
      dest = srcs.map {|src| File.join(dest, File.basename(src))} if @made_dirs[dest]
      @installed_list.puts dest
    end
  end

  def ln_sf(src, dest)
    super(src, with_destdir(dest))
    @installed_list.puts dest if @installed_list
  end

  def makedirs(dirs)
    dirs = fu_list(dirs)
    dirs.collect! do |dir|
      realdir = with_destdir(dir)
      realdir unless @made_dirs.fetch(dir) do
        @made_dirs[dir] = true
        @installed_list.puts(File.join(dir, "")) if @installed_list
        File.directory?(realdir)
      end
    end.compact!
    super(dirs, :mode => @dir_mode) unless dirs.empty?
  end

  FalseProc = proc {false}

  def path_matcher(pat)
    if pat and !pat.empty?
      proc {|f| pat.any? {|n| File.fnmatch?(n, f)}}
    else
      FalseProc
    end
  end

  def install_recursive(srcdir, dest, options = {})
    opts = options.clone
    noinst = opts.delete(:no_install)
    glob = opts.delete(:glob) || "*"
    maxdepth = opts.delete(:maxdepth)
    subpath = (srcdir.size+1)..-1
    prune = []
    skip = []
    if noinst
      if Array === noinst
        prune = noinst.grep(/#{File::SEPARATOR}/o).map!{|f| f.chomp(File::SEPARATOR)}
        skip = noinst.grep(/\A[^#{File::SEPARATOR}]*\z/o)
      else
        if noinst.index(File::SEPARATOR)
          prune = [noinst]
        else
          skip = [noinst]
        end
      end
    end
    skip |= %w"#*# *~ *.old *.bak *.orig *.rej *.diff *.patch *.core"
    prune = path_matcher(prune)
    skip = path_matcher(skip)
    File.directory?(srcdir) or return rescue return
    paths = [[srcdir, dest, 0]]
    found = []
    while file = paths.shift
      found << file
      file, d, dir = *file
      if dir
        depth = dir + 1
        next if maxdepth and maxdepth < depth
        files = []
        Dir.foreach(file) do |f|
          src = File.join(file, f)
          d = File.join(dest, dir = src[subpath])
          stat = File.lstat(src) rescue next
          if stat.directory?
            files << [src, d, depth] if maxdepth != depth and /\A\./ !~ f and !prune[dir]
          elsif stat.symlink?
            # skip
          else
            files << [src, d, false] if File.fnmatch?(glob, f) and !skip[f]
          end
        end
        paths.insert(0, *files)
      end
    end
    for src, d, dir in found
      if dir
        makedirs(d)
      else
        makedirs(d[/.*(?=\/)/m])
        if block_given?
          yield src, d, opts
        else
          install src, d, opts
        end
      end
    end
  end

  def open_for_install(path, mode)
    data = open(realpath = with_destdir(path), "rb") {|f| f.read} rescue nil
    newdata = yield
    unless @dryrun
      unless newdata == data
        open(realpath, "wb", mode) {|f| f.write newdata}
      end
      File.chmod(mode, realpath)
    end
    @installed_list.puts path if @installed_list
  end

  def with_destdir(dir)
    return dir if !@destdir or @destdir.empty?
    dir = dir.sub(/\A\w:/, '') if File::PATH_SEPARATOR == ';'
    @destdir + dir
  end

  def prepare(mesg, basedir, subdirs=nil)
    return unless basedir
    case
    when !subdirs
      dirs = basedir
    when subdirs.size == 0
      subdirs = nil
    when subdirs.size == 1
      dirs = [basedir = File.join(basedir, subdirs)]
      subdirs = nil
    else
      dirs = [basedir, *subdirs.collect {|dir| File.join(basedir, dir)}]
    end
    printf("installing %-18s %s%s\n", "#{mesg}:", basedir,
      (subdirs ? " (#{subdirs.join(', ')})" : ""))
    makedirs(dirs)
  end
end
