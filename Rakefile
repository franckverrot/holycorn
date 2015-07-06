require 'fileutils'

MRubyRCompilationFailed = Class.new(RuntimeError)

desc "Build mruby in a vendor directory"
task :vendor_mruby do
  FileUtils.mkdir_p('vendor')
  chdir('vendor') do
    sh "git clone git@github.com:mruby/mruby.git"
    chdir('mruby') do
      FileUtils.cp('../../config/build_config.rb', '.')
      sh "make"
    end
  end
end

desc "Clean vendor"
task :clean do
  FileUtils.rm_rf('vendor')
end

task :default => [:clean, :vendor_mruby]
