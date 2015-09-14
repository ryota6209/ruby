features = RbConfig::Features
if features['gems']
  require 'rubygems.rb'
  begin
    require 'did_you_mean'
  rescue LoadError
  end if features['did_you_mean']
end
