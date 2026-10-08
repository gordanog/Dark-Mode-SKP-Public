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

  def self.set_enabled(enabled)
    enabled ? apply : restore
  end

  def self.apply
    model = Sketchup.active_model
    options = model.rendering_options
    ensure_snapshot(model, options)

    options['BackgroundColor'] = Sketchup::Color.new(46, 46, 46)
    options['FaceFrontColor'] = Sketchup::Color.new(204, 204, 204)
    options['HighlightColor'] = Sketchup::Color.new(125, 183, 255)
    options['DrawHorizon'] = false
    options['DrawGround'] = false
    model.active_view.invalidate
    [true, nil]
  rescue => error
    [false, "#{error.class}: #{error.message}"]
  end

  def self.restore
    model = Sketchup.active_model
    snapshot = load_snapshot(model)
    return [true, nil] unless snapshot

    options = model.rendering_options
    snapshot.each do |key, value|
      options[key] = deserialise_option(value)
    end
    model.delete_attribute(SNAPSHOT_DICTIONARY, SNAPSHOT_KEY)
    model.active_view.invalidate
    [true, nil]
  rescue => error
    [false, "#{error.class}: #{error.message}"]
  end

  def self.save_snapshot(model, options)
    snapshot = {}
    STYLE_KEYS.each do |key|
      snapshot[key] = serialise_option(options[key])
    end
    model.set_attribute(SNAPSHOT_DICTIONARY, SNAPSHOT_KEY, JSON.generate(snapshot))
  end

  def self.ensure_snapshot(model, options)
    snapshot = load_snapshot(model)
    return save_snapshot(model, options) unless snapshot

    changed = false
    STYLE_KEYS.each do |key|
      next if snapshot.key?(key)

      snapshot[key] = serialise_option(options[key])
      changed = true
    end
    model.set_attribute(SNAPSHOT_DICTIONARY, SNAPSHOT_KEY, JSON.generate(snapshot)) if changed
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

end
end
end