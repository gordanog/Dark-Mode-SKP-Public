# DarkModeSKP - Global settings persistence manager

require 'json'
require 'fileutils'

module AG
module DarkModeSKP
module GlobalSettings

  GLOBAL_KEYS = [
    :dark_mode_enabled,
    :apply_dark_style
  ].freeze

  def self.settings_path
    if Sketchup.platform == :platform_win
      appdata = ENV['APPDATA'] || File.expand_path('~/AppData/Roaming')
    else
      appdata = File.expand_path('~/Library/Application Support')
    end

    dir = File.join(appdata, 'DarkModeSKP')
    FileUtils.mkdir_p(dir) unless File.exist?(dir)
    File.join(dir, 'settings.json')
  end

  def self.load
    path = settings_path
    return {} unless File.exist?(path)

    parsed = JSON.parse(File.read(path, encoding: 'UTF-8'))
    out = {}
    parsed.each do |key, value|
      symbol_key = key.to_sym
      next unless GLOBAL_KEYS.include?(symbol_key)

      out[symbol_key] = normalise_value(symbol_key, value)
    end
    out
  rescue
    {}
  end

  def self.save(settings)
    out = {}
    GLOBAL_KEYS.each do |key|
      next unless settings.key?(key)

      out[key.to_s] = normalise_value(key, settings[key])
    end
    File.write(settings_path, JSON.pretty_generate(out), encoding: 'UTF-8')
    true
  rescue
    false
  end

  def self.merge_with_defaults(defaults)
    defaults.merge(load)
  end

  def self.normalise_value(_key, value)
    value == true || value.to_s == 'true'
  end

end
end
end
