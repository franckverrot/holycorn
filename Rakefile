require 'fileutils'

MRubyRCompilationFailed = Class.new(RuntimeError)

desc "Vendor mruby in a vendor directory"
task :vendor_mruby do
  FileUtils.mkdir_p('vendor')
  chdir('vendor') do
    sh "git clone --depth=1 https://github.com/mruby/mruby.git"
  end
end

desc "Build mruby"
task :build_mruby do
  chdir('vendor/mruby') do
    FileUtils.cp('../../config/build_config.rb', '.')
    sh "make"
  end
end

desc "Clean vendor"
task :clean do
  FileUtils.rm_rf('vendor')
end

task :default => [:clean, :vendor_mruby, :build_mruby]
