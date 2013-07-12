class RbInstall
  def install_binary_commands
    prepare "binary commands", @bindir

    install @ruby_install_name+@exeext, @bindir, :mode => @prog_mode, :strip => @strip
    if @rubyw_install_name and !@rubyw_install_name.empty?
      install @rubyw_install_name+@exeext, @bindir, :mode => @prog_mode, :strip => @strip
    end
    if File.exist? @goruby_install_name+@exeext
      install @goruby_install_name+@exeext, @bindir, :mode => @prog_mode, :strip => @strip
    end
    if @enable_shared and @dll != @lib
      install @dll, @bindir, :mode => @prog_mode, :strip => @strip
    end
  end

  def install_base_libraries
    prepare "base libraries", @libdir

    install @lib, @libdir, :mode => @prog_mode, :strip => @strip unless @lib == @arc
    install @arc, @libdir, :mode => @data_mode
    if @dll == @lib and @dll != @arc
      for link in CONFIG["LIBRUBY_ALIASES"].split
        ln_sf(@dll, File.join(@libdir, link))
      end
    end

    prepare "arch files", @archlibdir
    install "rbconfig.rb", @archlibdir, :mode => @data_mode
    if CONFIG["ARCHFILE"]
      for file in CONFIG["ARCHFILE"].split
        install file, @archlibdir, :mode => @data_mode
      end
    end
  end

  def install_data
    pc = CONFIG["ruby_pc"]
    if pc and File.file?(pc) and File.size?(pc)
      prepare "pkgconfig data", pkgconfigdir = File.join(@libdir, "pkgconfig")
      install pc, pkgconfigdir, :mode => @data_mode
    end
  end

  def install_arch_extension_objects
    prepare "extension objects", @archlibdir
    noinst = %w[-* -*/] | (CONFIG["no_install_files"] || "").split
    install_recursive("#{@extout}/#{@arch}", @archlibdir, :no_install => noinst, :mode => @prog_mode, :strip => @strip)
    prepare "extension objects", @sitearchlibdir
    prepare "extension objects", @vendorarchlibdir
  end

  def install_arch_extension_headers
    prepare "extension headers", @archhdrdir
    install_recursive("#{@extout}/include/#{@arch}", @archhdrdir, :glob => "*.h", :mode => @data_mode)
  end

  def install_comm_extension_scripts
    prepare "extension scripts", @rubylibdir
    install_recursive("#{@extout}/common", @rubylibdir, :mode => @data_mode)
    prepare "extension scripts", @sitelibdir
    prepare "extension scripts", @vendorlibdir
  end

  def install_comm_extension_headers
    hdrdir = @rubyhdrdir + "/ruby"
    prepare "extension headers", hdrdir
    install_recursive("#{@extout}/include/ruby", hdrdir, :glob => "*.h", :mode => @data_mode)
  end

  def install_rdoc
    if @rdocdir
      ridatadir = File.join(CONFIG['ridir'], CONFIG['ruby_version'], "system")
      prepare "rdoc", ridatadir
      install_recursive(@rdocdir, ridatadir, :mode => @data_mode)
    end
  end

  def install_capi_docs
    prepare "capi-docs", @docdir
    install_recursive "doc/capi", @docdir+"/capi", :mode => @data_mode
  end

  def install_command_scripts
    prepare "command scripts", @bindir

    ruby_shebang = File.join(@bindir, @ruby_install_name)
    if File::ALT_SEPARATOR
      ruby_bin = ruby_shebang.tr(File::SEPARATOR, File::ALT_SEPARATOR)
      if @cmdtype == 'exe'
        stub = File.open("rubystub.exe", "rb") {|f| f.read} << "\n" rescue nil
      end
    end
    if trans = CONFIG["program_transform_name"]
      exp = []
      trans.gsub!(/\$\$/, '$')
      trans.scan(%r[\G[\s;]*(/(?:\\.|[^/])*/)?([sy])(\\?\W)((?:(?!\3)(?:\\.|.))*)\3((?:(?!\3)(?:\\.|.))*)\3([gi]*)]) do
        |addr, cmd, sep, pat, rep, opt|
        addr &&= Regexp.new(addr[/\A\/(.*)\/\z/, 1])
        case cmd
        when 's'
          next if pat == '^' and rep.empty?
          exp << [addr, (opt.include?('g') ? :gsub! : :sub!),
                  Regexp.new(pat, opt.include?('i')), rep.gsub(/&/){'\&'}]
        when 'y'
          exp << [addr, :tr!, Regexp.quote(pat), rep]
        end
      end
      trans = proc do |base|
        exp.each {|addr, opt, pat, rep| base.__send__(opt, pat, rep) if !addr or addr =~ base}
        base
      end
    elsif /ruby/ =~ @ruby_install_name
      trans = proc {|base| @ruby_install_name.sub(/ruby/, base)}
    else
      trans = proc {|base| base}
    end
    install_recursive(File.join(@@srcdir, "bin"), @bindir, :maxdepth => 1) do |src, cmd|
      cmd = cmd.sub(/[^\/]*\z/m) {|n| RbConfig.expand(trans[n])}

      shebang = ''
      body = ''
      open(src, "rb") do |f|
        shebang = f.gets
        body = f.read
      end
      shebang or raise "empty file - #{src}"
      if @prolog_script
        shebang.sub!(/\A(\#!.*?ruby\b)?/) {@prolog_script + ($1 || "#!ruby\n")}
      else
        shebang.sub!(/\A\#!.*?ruby\b/) {"#!" + ruby_shebang}
      end
      shebang.sub!(/\r$/, '')
      body.gsub!(/\r$/, '')

      cmd << ".#{@cmdtype}" if @cmdtype
      open_for_install(cmd, @script_mode) do
        case @cmdtype
        when "exe"
          stub + shebang + body
        when "bat"
          [<<-"EOH".gsub(/^\s+/, ''), shebang, body, "__END__\n:endofruby\n"].join.gsub(/$/, "\r")
        @echo off
        @if not "%~d0" == "~d0" goto WinNT
        #{ruby_bin} -x "#{cmd}" %1 %2 %3 %4 %5 %6 %7 %8 %9
        @goto endofruby
        :WinNT
        "%~dp0#{@ruby_install_name}" -x "%~f0" %*
        @goto endofruby
          EOH
        when "cmd"
          <<"/EOH" << shebang << body
@"%~dp0#{@ruby_install_name}" -x "%~f0" %*
@exit /b %ERRORLEVEL%
/EOH
        else
          shebang + body
        end
      end
    end
  end

  def install_library_scripts
    prepare "library scripts", @rubylibdir
    noinst = %w[README* *.txt *.rdoc *.gemspec]
    install_recursive(File.join(@@srcdir, "lib"), @rubylibdir, :no_install => noinst, :mode => @data_mode)
  end

  def install_common_headers
    prepare "common headers", @rubyhdrdir

    noinst = []
    unless RUBY_PLATFORM =~ /mswin|mingw|bccwin/
      noinst << "win32.h"
    end
    noinst = nil if noinst.empty?
    install_recursive(File.join(@@srcdir, "include"), @rubyhdrdir, :no_install => noinst, :glob => "*.h", :mode => @data_mode)
  end

  def install_manpages
    mdocs = Dir["#{@@srcdir}/man/*.[1-9]"]
    prepare "manpages", @mandir, ([] | mdocs.collect {|mdoc| mdoc[/\d+$/]}).sort.collect {|sec| "man#{sec}"}

    @mandir = File.join(@mandir, "man")
    has_goruby = File.exist?(@goruby_install_name+@exeext)
    require File.join(@@srcdir, "tool/mdoc2man.rb") if @mantype != "doc"
    mdocs.each do |mdoc|
      next unless File.file?(mdoc) and open(mdoc){|fh| fh.read(1) == '.'}
      base = File.basename(mdoc)
      if base == "goruby.1"
        next unless has_goruby
      end

      destdir = @mandir + (section = mdoc[/\d+$/])
      destname = @ruby_install_name.sub(/ruby/, base.chomp(".#{section}"))
      destfile = File.join(destdir, "#{destname}.#{section}")

      if @mantype == "doc"
        install mdoc, destfile, :mode => @data_mode
      else
        class << (w = [])
          alias print push
        end
        open(mdoc) {|r| Mdoc2Man.mdoc2man(r, w)}
        open_for_install(destfile, @data_mode) {w.join("")}
      end
    end
  end

  def install_default_gems
    require("rubygems.rb")
    gem_dir = Gem.default_dir
    directories = Gem.ensure_gem_subdirectories(gem_dir, :mode => @dir_mode)
    prepare "default gems", gem_dir, directories

    spec_dir = File.join(gem_dir, directories.grep(/^spec/)[0])
    default_spec_dir = "#{spec_dir}/default"
    makedirs(default_spec_dir)

    gems = {}
    File.foreach(File.join(@@srcdir, "defs/default_gems")) do |line|
      line.chomp!
      line.sub!(/\s*#.*/, '')
      next if line.empty?
      words = []
      line.scan(/\G\s*([^\[\]\s]+|\[([^\[\]]*)\])/) do
        words << ($2 ? $2.split : $1)
      end
      name, base_dir, src, execs = *words
      next unless name and base_dir and src

      src       = File.join(@@srcdir, src)
      base_dir  = File.join(@@srcdir, base_dir)
      specgen   = RbInstall::Specs::Generator.new(name, base_dir, src, execs || [], @extout, @arch)
      gems[name] ||= specgen
    end

    Dir.glob(@@srcdir+"/{lib,ext}/**/*.gemspec").each do |src|
      specgen   = RbInstall::Specs::Reader.new(src, @extout, @arch)
      gems[specgen.gemspec.name] ||= specgen
    end

    gems.sort.each do |name, specgen|
      gemspec   = specgen.gemspec
      base_dir  = specgen.src.sub(/\A#{Regexp.escape(@@srcdir)}\//, "")
      full_name = "#{gemspec.name}-#{gemspec.version}"

      puts "#{" "*30}#{gemspec.name} #{gemspec.version}"
      gemspec_path = File.join(default_spec_dir, "#{full_name}.gemspec")
      open_for_install(gemspec_path, @data_mode) do
        specgen.spec_source
      end

      unless gemspec.executables.empty? then
        bin_dir = File.join(gem_dir, 'gems', full_name, 'bin')
        makedirs(bin_dir)

        execs = gemspec.executables.map {|exec| File.join(@@srcdir, 'bin', exec)}
        install(execs, bin_dir, :mode => @prog_mode)
      end
    end
  end
end
