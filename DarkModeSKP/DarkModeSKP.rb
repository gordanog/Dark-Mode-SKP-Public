# DarkModeSKP.rb
# Root extension file - registers the extension with SketchUp

require 'sketchup.rb'
require 'extensions.rb'

module AG
  module DarkModeSKP

    unless file_loaded?(__FILE__)
      EXTENSION = SketchupExtension.new('Dark Mode SKP', 'DarkModeSKP/main')

      EXTENSION.version     = '0.1.0'
      EXTENSION.creator     = 'gordanog'
      EXTENSION.description = 'Applies a dark theme to SketchUp Qt and native Windows UI in SketchUp 2024 or newer for Windows.'

      Sketchup.register_extension(EXTENSION, true)

      file_loaded(__FILE__)
    end

  end
end
