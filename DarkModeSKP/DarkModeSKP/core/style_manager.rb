require 'json'

module AG
module DarkModeSKP
module StyleManager

  SNAPSHOT_DICTIONARY = 'AG_DarkModeSKP'.freeze
  SNAPSHOT_KEY = 'dark_style_snapshot'.freeze
  STYLE_KEYS = [
    'BackgroundColor',
    'FaceFrontColor',
    'HighlightColor',
    'DrawHorizon',
    'DrawGround'
  ].freeze
  DARK_STYLE = {
    'BackgroundColor' => Sketchup::Color.new(46, 46, 46),
    'FaceFrontColor' => Sketchup::Color.new(204, 204, 204),
    'HighlightColor' => Sketchup::Color.new(125, 183, 255),
    'DrawHorizon' => false,
    'DrawGround' => false
  }.freeze

  def self.set_enabled(enabled)
    enabled ? apply : restore
  end

  def self.apply(model = Sketchup.active_model)
    options = model.rendering_options
    session = session_for(model, options)
    changed = apply_options(options, DARK_STYLE)
    session[:applied] = true
    model.active_view.invalidate if changed
    [true, nil]
  rescue => error
    [false, "#{error.class}: #{error.message}"]
  end

  def self.restore(model = Sketchup.active_model)
    session = session_for_existing(model)
    snapshot = session ? session[:snapshot] : load_snapshot(model)
    return [true, nil] unless snapshot

    options = model.rendering_options
    changed = apply_options(options, snapshot)
    clear_session(model)
    clear_persisted_snapshot(model)
    model.active_view.invalidate if changed
    [true, nil]
  rescue => error
    [false, "#{error.class}: #{error.message}"]
  end

  def self.prepare_for_save(model)
    session = session_for_existing(model)
    return false unless session && session[:applied]

    changed = apply_options(model.rendering_options, session[:snapshot])
    session[:applied] = false
    session[:restore_after_save] = true
    model.active_view.invalidate if changed
    true
  rescue
    false
  end

  def self.restore_after_save(model)
    session = session_for_existing(model)
    return [true, nil] unless session && session.delete(:restore_after_save)

    apply(model)
  end

  def self.clear_session(model)
    sessions.delete(model.object_id)
  end

  def self.dark_style_applied?(model)
    session = session_for_existing(model)
    session && session[:applied]
  end

  def self.load_snapshot(model)
    raw_snapshot = model.get_attribute(SNAPSHOT_DICTIONARY, SNAPSHOT_KEY)
    return nil if raw_snapshot.nil? || raw_snapshot.to_s.empty?

    snapshot = JSON.parse(raw_snapshot.to_s)
    snapshot.is_a?(Hash) ? snapshot : nil
  rescue JSON::ParserError, TypeError
    nil
  end

  def self.serialise_option(value)
    return value unless value.is_a?(Sketchup::Color)

    {
      'type' => 'color',
      'red' => value.red,
      'green' => value.green,
      'blue' => value.blue,
      'alpha' => value.alpha
    }
  end

  def self.deserialise_option(value)
    return value unless value.is_a?(Hash) && value['type'] == 'color'

    Sketchup::Color.new(value['red'].to_i, value['green'].to_i, value['blue'].to_i, value['alpha'].to_i)
  end

  def self.sessions
    @sessions ||= {}
  end

  def self.session_for(model, options)
    existing_session = session_for_existing(model)
    return existing_session if existing_session

    persisted_snapshot = load_snapshot(model)
    session = {
      model: model,
      snapshot: persisted_snapshot || capture_snapshot(options),
      applied: false
    }
    sessions[model.object_id] = session
    clear_persisted_snapshot(model) if persisted_snapshot
    session
  end

  def self.session_for_existing(model)
    session = sessions[model.object_id]
    session if session && session[:model].equal?(model)
  end

  def self.capture_snapshot(options)
    STYLE_KEYS.each_with_object({}) do |key, snapshot|
      snapshot[key] = serialise_option(options[key])
    end
  end

  def self.clear_persisted_snapshot(model)
    model.delete_attribute(SNAPSHOT_DICTIONARY, SNAPSHOT_KEY)
  end

  def self.apply_options(options, values)
    changed = false
    values.each do |key, value|
      value = deserialise_option(value)
      next if matching_option_value?(options[key], value)

      options[key] = value
      changed = true
    end
    changed
  end

  def self.matching_option_value?(first, second)
    return first == second unless first.is_a?(Sketchup::Color) && second.is_a?(Sketchup::Color)

    first.red == second.red &&
      first.green == second.green &&
      first.blue == second.blue &&
      first.alpha == second.alpha
  end

end
end
end