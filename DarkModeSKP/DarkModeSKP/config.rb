# DarkModeSKP - Configuration & Default Settings

module AG
module DarkModeSKP

  PLUGIN_NAME = 'Dark Mode SKP'
  PLUGIN_ID   = 'com.ag.dark_mode_skp'
  VERSION     = '0.1.0'
  MINIMUM_SKETCHUP_VERSION = 2024
  UNSUPPORTED_ENVIRONMENT_MESSAGE = "Supports only SketchUp #{MINIMUM_SKETCHUP_VERSION}+ on Windows"

  DEFAULT_SETTINGS = {
    dark_mode_enabled: false,
    apply_dark_style: false
  }.freeze

end
end
