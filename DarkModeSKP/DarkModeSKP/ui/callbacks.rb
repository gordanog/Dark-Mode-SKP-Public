# DarkModeSKP - HtmlDialog callback registrations

require 'json'

module AG
module DarkModeSKP

  DIALOG_FUNCTIONS = %w[
    refreshDialogState
    showStatusMessage
  ].freeze

  def self.execute_dialog_function(function_name, *args)
    return unless @dialog

    function = function_name.to_s
    return unless DIALOG_FUNCTIONS.include?(function)

    @dialog.execute_script("#{function}(#{args.map(&:to_json).join(', ')});")
  end

  def self.show_dialog_status_message(message, status_type = 'info')
    execute_dialog_function('showStatusMessage', message.to_s, status_type.to_s)
  end

  def self.setup_callbacks
    return unless @dialog

    @dialog.add_action_callback('dialogReady') do |_context|
      refresh_dialog_state
    end

    @dialog.add_action_callback('resizeDialog') do |_context, content_height, viewport_height|
      resize_dialog_to_content(content_height, viewport_height)
    end

    @dialog.add_action_callback('setDarkMode') do |_context, enabled|
      enabled = truthy?(enabled)
      ok, message = set_dark_mode_startup_enabled(enabled)
      refresh_dialog_state
      show_dialog_status_message(message, 'error') unless ok
    end

    @dialog.add_action_callback('updateSettings') do |_context, json|
      settings = parse_settings_json(json)
      ok, message = update_settings(settings)
      refresh_dialog_state
      show_dialog_status_message(message, 'error') unless ok
    end
  end

  def self.parse_settings_json(json)
    parsed = JSON.parse(json.to_s)
    return {} unless parsed.is_a?(Hash)

    out = {}
    parsed.each do |key, value|
      symbol_key = key.to_sym
      next unless [:apply_dark_style].include?(symbol_key)

      out[symbol_key] = truthy?(value)
    end
    out
  rescue JSON::ParserError, TypeError
    {}
  end

  def self.truthy?(value)
    value == true || value.to_s == 'true' || value.to_s == '1'
  end

end
end
