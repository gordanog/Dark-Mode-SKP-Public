# DarkModeSKP - Native Qt dark-mode helper bridge

require 'fiddle'

module AG
module DarkModeSKP
module RuntimeBridge

  NATIVE_HELPER_NAME = Sketchup.platform == :platform_win ? 'main.dll' : nil
  MAX_INFO_BYTES = 2048

  def self.native_helper_path
    return nil unless NATIVE_HELPER_NAME

    @native_helper_path ||= begin
      path = File.expand_path(NATIVE_HELPER_NAME, File.dirname(__dir__))
      File.exist?(path) ? path : nil
    end
  end

  def self.available?
    path = native_helper_path
    !!(path && File.exist?(path))
  end

  def self.load_native_helper!
    @native_helper_library ||= begin
      path = native_helper_path
      raise 'Dark Mode SKP native helper is missing.' unless path && File.exist?(path)

      Fiddle.dlopen(path)
    end
  end

  def self.native_helper_function(name, args, result)
    @native_helper_functions ||= {}
    @native_helper_functions[name] ||= begin
      dll = load_native_helper!
      Fiddle::Function.new(dll[name], args, result)
    end
  end

  def self.set_dark_mode_function
    native_helper_function('darkmode_skp_set_dark_mode', [Fiddle::TYPE_INT], Fiddle::TYPE_INT)
  end

  def self.initialize_native_helper_function
    native_helper_function('darkmode_skp_initialize', [], Fiddle::TYPE_INT)
  end

  def self.dark_mode_applied_function
    native_helper_function('darkmode_skp_dark_mode_applied', [], Fiddle::TYPE_INT)
  end

  def self.theme_info_function
    native_helper_function('darkmode_skp_theme_info', [Fiddle::TYPE_VOIDP, Fiddle::TYPE_INT], Fiddle::TYPE_INT)
  end

  def self.set_dark_mode(enabled)
    return [false, 'Dark Mode SKP native helper is missing.'] unless available?

    rc = set_dark_mode_function.call(enabled ? 1 : 0).to_i
    rc == 1 ? [true, nil] : [false, theme_info]
  rescue => error
    [false, "#{error.class}: #{error.message}"]
  end

  def self.initialize_native_helper
    return false unless available?

    initialize_native_helper_function.call.to_i == 1
  rescue
    false
  end

  def self.dark_mode_applied?
    return false unless available?

    dark_mode_applied_function.call.to_i == 1
  rescue
    false
  end

  def self.theme_info
    return '' unless available?

    buffer = Fiddle::Pointer.malloc(MAX_INFO_BYTES)
    buffer[0, MAX_INFO_BYTES] = "\0" * MAX_INFO_BYTES
    theme_info_function.call(buffer, MAX_INFO_BYTES)
    buffer.to_s(MAX_INFO_BYTES).split("\0", 2).first.to_s.strip
  rescue
    ''
  end

end
end
end
