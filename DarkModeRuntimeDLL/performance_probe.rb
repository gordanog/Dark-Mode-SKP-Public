# Development-only startup script for the instrumented performance_probe.cpp DLL.
# Launch SketchUp with -RubyStartup and this file's absolute path.
# Both output/control paths must be supplied explicitly by the caller.
require 'json'
require 'fiddle'

module DarkModePerformanceProbe
  def self.start
    @output_path = ENV.fetch('DARKMODE_PERF_OUTPUT')
    @control_path = ENV.fetch('DARKMODE_PERF_CONTROL')
    bridge = AG::DarkModeSKP::RuntimeBridge
    @begin_probe = bridge.native_helper_function('darkmode_skp_perf_begin', [Fiddle::TYPE_INT], Fiddle::TYPE_INT)
    @read_probe = bridge.native_helper_function('darkmode_skp_perf_info', [Fiddle::TYPE_VOIDP, Fiddle::TYPE_INT], Fiddle::TYPE_INT)
    lookup_probe = bridge.native_helper_function('darkmode_skp_perf_lookup', [Fiddle::TYPE_INT], Fiddle::TYPE_DOUBLE)
    @lookup_ms = lookup_probe.call(100)
    if ENV['DARKMODE_PERF_FIXTURE'] == '1'
      model = Sketchup.active_model
      raise 'Performance fixture requires an empty, unsaved model' unless model.path.empty? && model.entities.length == 0
      group = model.entities.add_group
      face = group.entities.add_face([0, 0, 0], [100, 0, 0], [100, 100, 0], [0, 100, 0])
      face.pushpull(100)
      model.pages.add('Performance scene 1')
      model.pages.add('Performance scene 2')
      model.active_view.zoom_extents
    end
    @buffer = Fiddle::Pointer.malloc(2048)
    @generation = nil
    @timer = UI.start_timer(0.25, true) { sample }
    sample
  rescue => error
    File.write(@output_path, JSON.generate(error: "#{error.class}: #{error.message}")) if @output_path
  end

  def self.sample
    generation = File.read(@control_path).strip
    if generation != @generation
      mode = generation.split(':', 2).first.to_i
      raise 'Could not install timing hooks' unless @begin_probe.call(mode) == 1
      @generation = generation
    end
    @read_probe.call(@buffer, 2048)
    sample = JSON.parse(@buffer.to_s)
    sample[:generation] = @generation
    sample[:lookup_100_ms] = @lookup_ms
    sample[:sketchup] = Sketchup.version
    sample[:dark] = AG::DarkModeSKP::RuntimeBridge.dark_mode_applied?
    File.write(@output_path, JSON.generate(sample))
  rescue => error
    File.write(@output_path, JSON.generate(error: "#{error.class}: #{error.message}"))
  end
end

UI.start_timer(1.0, false) { DarkModePerformanceProbe.start }
