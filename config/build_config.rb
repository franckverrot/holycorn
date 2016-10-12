def embed_gems(conf)
  # Embedded gems
  conf.gem :git => 'https://github.com/matsumoto-r/mruby-httprequest.git'
  conf.gem :git => 'https://github.com/mattn/mruby-json.git'
  conf.gem :git => 'https://github.com/iij/mruby-io.git'
  conf.gem :git => 'https://github.com/matsumoto-r/mruby-redis.git'

  # Builtin Foreign Data Wrappers
  conf.gem '../../builtin_wrappers/holycorn-redis'

  # Include the default GEMs
  conf.gembox 'default'

  [conf.cc, conf.cxx, conf.linker].each do |cc|
    cc.flags << "-fPIC"
  end
  # Include any mruby specific configuration here
  # N/A
end

MRuby::Build.new do |conf|
  toolchain :gcc

  enable_debug

  embed_gems(conf)
end

MRuby::CrossBuild.new('i686-pc-linux-gnu') do |conf|
  toolchain :gcc
  enable_debug
  [conf.cc, conf.cxx, conf.linker].each do |cc|
    cc.flags << "-fPIC"
  end
  embed_gems(conf)
end
