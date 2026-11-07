# DarkModeSKP - Main entry point

require 'sketchup.rb'
require 'json'

module AG
module DarkModeSKP

  def self.require_component(sketchup_path, relative_path)
    Sketchup.require(sketchup_path)
  rescue LoadError
    require_relative relative_path
  end

  require_component('DarkModeSKP/config', 'config')
  require_component('DarkModeSKP/global_settings', 'global_settings')
  require_component('DarkModeSKP/core/runtime_bridge', 'core/runtime_bridge')
  require_component('DarkModeSKP/core/style_manager', 'core/style_manager')
  require_component('DarkModeSKP/ui/dialog', 'ui/dialog')
  require_component('DarkModeSKP/ui/callbacks', 'ui/callbacks')

  @settings ||= GlobalSettings.merge_with_defaults(DEFAULT_SETTINGS.dup)
  @dialog ||= nil
  @startup_applied ||= false
  @dark_style_model_observers ||= {}
  @dark_style_saving_models ||= {}
  @dark_style_frame_observer ||= nil
  @dark_style_frame_observer_id ||= nil

  class DarkStyleAppObserver < Sketchup::AppObserver
    def onNewModel(model)
      AG::DarkModeSKP.schedule_dark_style_apply(model)
    end

    def onOpenModel(model)
      AG::DarkModeSKP.schedule_dark_style_apply(model)
    end

    def onCloseModel(model)
      AG::DarkModeSKP.remove_dark_style_observers(model)
    end
  end

  class DarkStyleModelObserver < Sketchup::ModelObserver
    def onPreSaveModel(model)
      AG::DarkModeSKP.prepare_dark_style_for_save(model)
    end

    def onPostSaveModel(model)
      AG::DarkModeSKP.restore_dark_style_after_save(model)
    end
  end

  class DarkStyleFrameObserver
    def frameChange(_from_page, to_page, _percent_done)
      AG::DarkModeSKP.apply_dark_style_for_scene_frame(to_page)
    end
  end

  def self.current_settings
    @settings.dup
  end

  def self.sketchup_release_year
    major_version = Sketchup.version.to_s.split('.', 2).first.to_i
    major_version >= 2000 ? major_version : 2000 + major_version
  end

  def self.supported_environment?
    Sketchup.platform == :platform_win && sketchup_release_year >= MINIMUM_SKETCHUP_VERSION
  end

  def self.update_settings(settings)
    return [false, UNSUPPORTED_ENVIRONMENT_MESSAGE] unless supported_environment?

    if settings.key?(:apply_dark_style)
      @settings[:apply_dark_style] = truthy?(settings[:apply_dark_style])
    end

    persist_settings
    [true, nil]
  end

  def self.set_dark_mode_startup_enabled(enabled)
    return [false, UNSUPPORTED_ENVIRONMENT_MESSAGE] unless supported_environment?

    @settings[:dark_mode_enabled] = truthy?(enabled)
    persist_settings
    [true, nil]
  end

  def self.set_dark_style_enabled(enabled)
    return StyleManager.restore unless truthy?(enabled)
    return [true, nil] unless truthy?(@settings[:apply_dark_style])

    install_dark_style_observers(Sketchup.active_model)
    StyleManager.apply
  end

  def self.schedule_dark_style_apply(model)
    return unless model && supported_environment?

    install_dark_style_observers(model)
    schedule_dark_style_reapply(model)
  rescue
    nil
  end

  def self.schedule_dark_style_reapply(model, active_model_only = true)
    return unless model && supported_environment?
    return if dark_style_save_in_progress?(model)

    UI.start_timer(0.0, false) do
      if (!active_model_only || model == Sketchup.active_model) &&
         !dark_style_save_in_progress?(model) &&
         dark_style_should_be_active?
        StyleManager.apply(model)
      end
    end
  rescue
    nil
  end

  def self.apply_dark_style_for_scene_frame(to_page)
    model = Sketchup.active_model
    return unless model && to_page
    return if dark_style_save_in_progress?(model)
    return unless dark_style_should_be_active?

    StyleManager.apply(model)
  end

  def self.prepare_dark_style_for_save(model)
    return unless model

    @dark_style_saving_models[model.object_id] = true if StyleManager.prepare_for_save(model)
  end

  def self.restore_dark_style_after_save(model)
    return unless @dark_style_saving_models.delete(model.object_id)

    schedule_dark_style_reapply(model, false)
  end

  def self.dark_style_should_be_active?
    RuntimeBridge.dark_mode_applied? && truthy?(@settings[:apply_dark_style])
  end

  def self.dark_style_save_in_progress?(model)
    @dark_style_saving_models.key?(model.object_id)
  end

  def self.install_dark_style_observers(model)
    return unless model

    model_key = model.object_id
    unless @dark_style_model_observers[model_key]
      observer = DarkStyleModelObserver.new
      model.add_observer(observer)
      @dark_style_model_observers[model_key] = observer
    end
    install_dark_style_frame_observer
  end

  def self.install_dark_style_frame_observer
    return if @dark_style_frame_observer_id

    @dark_style_frame_observer = DarkStyleFrameObserver.new
    @dark_style_frame_observer_id = Sketchup::Pages.add_frame_change_observer(@dark_style_frame_observer)
  end

  def self.remove_dark_style_observers(model)
    return unless model

    model_key = model.object_id
    model_observer = @dark_style_model_observers.delete(model_key)
    model.remove_observer(model_observer) if model_observer
    @dark_style_saving_models.delete(model_key)
    StyleManager.clear_session(model)
  rescue
    nil
  end

  def self.persist_settings
    GlobalSettings.save(@settings)
  end

  def self.open_dialog
    if @dialog && @dialog.visible?
      refresh_dialog_state
      @dialog.bring_to_front
      return
    end

    create_dialog
  rescue => error
    UI.messagebox("Failed to open #{PLUGIN_NAME}: #{error.message}")
  end

  def self.create_dialog
    @dialog = UI::HtmlDialog.new(
      {
        dialog_title: PLUGIN_NAME,
        preferences_key: PLUGIN_ID,
        scrollable: true,
        resizable: false,
        width: 420,
        height: 260,
        min_width: 420,
        max_width: 420,
        style: UI::HtmlDialog::STYLE_UTILITY
      }
    )
    @dialog.set_html(Dialog.generate_html(@settings, dialog_state))
    setup_callbacks
    @dialog.set_on_closed { @dialog = nil }
    @dialog.show
  end

  def self.resize_dialog_to_content(content_height, viewport_height)
    return unless @dialog

    content_height = content_height.to_i
    viewport_height = viewport_height.to_i
    return if content_height <= 0 || viewport_height <= 0

    _width, current_height = @dialog.get_size
    height = current_height.to_i + content_height - viewport_height
    @dialog.set_size(420, height)
  end

  def self.set_dark_mode_enabled(enabled, persist: true)
    return [false, UNSUPPORTED_ENVIRONMENT_MESSAGE] unless supported_environment?

    enabled = truthy?(enabled)
    ok, message = RuntimeBridge.set_dark_mode(enabled)
    if ok
      @settings[:dark_mode_enabled] = enabled
      persist_settings if persist
      install_dark_style_observers(Sketchup.active_model) if enabled
      style_ok, style_message = set_dark_style_enabled(enabled)
      refresh_dialog_state
      [style_ok, style_message]
    else
      [false, message]
    end
  end

  def self.dialog_state
    environment_supported = supported_environment?
    applied = environment_supported && RuntimeBridge.dark_mode_applied?
    dark_mode_enabled = truthy?(@settings[:dark_mode_enabled])
    {
      settings: @settings,
      environment_supported: environment_supported,
      unsupported_message: environment_supported ? nil : UNSUPPORTED_ENVIRONMENT_MESSAGE,
      dark_mode_enabled: dark_mode_enabled,
      dark_mode_applied: applied,
      restart_required: environment_supported && dark_mode_enabled != applied
    }
  end

  def self.refresh_dialog_state
    execute_dialog_function('refreshDialogState', dialog_state)
  end

  def self.install_dark_style_observer
    @dark_style_app_observer ||= DarkStyleAppObserver.new
    Sketchup.add_observer(@dark_style_app_observer)
  end

  def self.apply_startup_settings
    return if @startup_applied

    @startup_applied = true
    return unless supported_environment?

    apply_dark_mode = truthy?(@settings[:dark_mode_enabled])
    return unless apply_dark_mode

    UI.start_timer(0.0, false) do
      set_dark_mode_enabled(true, persist: false) if apply_dark_mode
    end
  rescue
    nil
  end

  def self.logo_icon_path
    icon_name = Sketchup.platform == :platform_osx ? 'logo.pdf' : 'logo.svg'
    extension_file = __FILE__.dup
    extension_file.force_encoding('UTF-8') if extension_file.respond_to?(:force_encoding)
    icon_path = File.join(File.dirname(extension_file), 'icons', icon_name)
    File.exist?(icon_path) ? icon_path : nil
  end

  def self.build_menu
    command = UI::Command.new('Dark Mode SKP Settings') { open_dialog }
    command.tooltip = 'Dark Mode SKP Settings'
    command.menu_text = 'Dark Mode SKP Settings'
    command.status_bar_text = 'Open Dark Mode SKP settings'

    icon_path = logo_icon_path
    if icon_path
      command.small_icon = icon_path
      command.large_icon = icon_path
    end

    UI.menu('Extensions').add_item(command)

    toolbar = UI::Toolbar.new('Dark Mode SKP')
    toolbar.add_item(command)
    toolbar.restore
  end

  def self.truthy?(value)
    value == true || value.to_s == 'true' || value.to_s == '1'
  end

  unless file_loaded?(__FILE__)
    if supported_environment?
      RuntimeBridge.initialize_native_helper
      RuntimeBridge.prepare_icon_theme(truthy?(@settings[:dark_mode_enabled]))
      install_dark_style_observer
      install_dark_style_observers(Sketchup.active_model)
    end
    build_menu
    apply_startup_settings
    file_loaded(__FILE__)
  end

end
end
