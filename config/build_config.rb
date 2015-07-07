MRuby::Build.new do |conf|
  # load specific toolchain settings

  # Gets set by the VS command prompts.
  if ENV['VisualStudioVersion'] || ENV['VSINSTALLDIR']
    toolchain :visualcpp
  else
    toolchain :gcc
  end

  enable_debug

  # Use any other gems here
  conf.gem :git => 'https://github.com/matsumoto-r/mruby-httprequest.git'
  conf.gem :git => 'https://github.com/mattn/mruby-json.git'
  conf.gem :git => 'https://github.com/iij/mruby-io.git'
  conf.gem :git => 'https://github.com/matsumoto-r/mruby-redis.git'

  # Include the default GEMs
  conf.gembox 'default'

  # Include any mruby specific configuration here
end

MRuby::Build.new('host-debug') do |conf|
  # load specific toolchain settings

  # Gets set by the VS command prompts.
  if ENV['VisualStudioVersion'] || ENV['VSINSTALLDIR']
    toolchain :visualcpp
  else
    toolchain :gcc
  end

  enable_debug

  # include the default GEMs
  conf.gembox 'default'

  # C compiler settings
  conf.cc.defines = %w(ENABLE_DEBUG)

  # Generate mruby debugger command (require mruby-eval)
  conf.gem :core => "mruby-bin-debugger"

  # bintest
  # conf.enable_bintest
end
