require 'minitest/autorun'
require 'open3'
require_relative '../DarkModeSKP/DarkModeSKP/config'
require_relative '../DarkModeSKP/DarkModeSKP/global_settings'

module UI
  class Notification
    class << self
      attr_accessor :notifications
    end

    attr_reader :arguments, :accept_label, :shown

    def initialize(*arguments)
      @arguments = arguments
      self.class.notifications << self
    end

    def on_accept(label, &block)
      @accept_label = label
      @accept_callback = block
    end

    def show
      @shown = true
    end
  end

  class Command
    attr_accessor :tooltip, :menu_text, :status_bar_text, :small_icon, :large_icon

    def initialize(_name); end
  end

  class Toolbar
    def initialize(_name); end
    def add_item(_command); end
    def restore; end
  end

  class Menu
    def add_item(_command); end
  end

  class << self
    attr_accessor :timers, :timer_sequence

    def menu(_name)
      Menu.new
    end

    def start_timer(_seconds, _repeat, &block)
      self.timer_sequence = timer_sequence.to_i + 1
      timers[timer_sequence] = block
      timer_sequence
    end

    def stop_timer(timer_id)
      timers.delete(timer_id)
    end
  end
end

module Sketchup
  class AppObserver; end
  class ModelObserver; end

  def self.version
    '26.2.243'
  end

  def self.platform
    :platform_win
  end

  def self.active_model
    nil
  end

  def self.add_observer(_observer); end

  def self.extensions
    [Struct.new(:name).new(AG::DarkModeSKP::PLUGIN_NAME)]
  end

  def self.require(path)
    return if %w[DarkModeSKP/global_settings DarkModeSKP/core/runtime_bridge DarkModeSKP/core/style_manager].include?(path)

    Kernel.require(File.expand_path("../DarkModeSKP/#{path}", __dir__))
  end

module Http
  GET = 'GET'

  class Request
    class << self
      attr_accessor :requests, :fail_start, :fail_create
    end

    attr_accessor :headers
    attr_reader :url, :method, :cancelled

    def initialize(url, method)
      raise 'Network unavailable' if self.class.fail_create

      @url = url
      @method = method
      self.class.requests << self
    end

    def start(&block)
      @callback = block
      !self.class.fail_start
    end

    def respond(status_code, body)
      @callback.call(self, Struct.new(:status_code, :body).new(status_code, body))
    end

    def cancel
      @cancelled = true
    end
  end
end
end

module AG::DarkModeSKP::RuntimeBridge
  def self.initialize_native_helper; end
  def self.prepare_icon_theme(_enabled); end

  def self.dark_mode_applied?
    false
  end
end

$dark_mode_test_loaded_files = []
$LOADED_FEATURES << 'sketchup.rb'

def file_loaded?(path)
  $dark_mode_test_loaded_files.include?(path)
end

def file_loaded(path)
  $dark_mode_test_loaded_files << path
end

require_relative '../DarkModeSKP/DarkModeSKP/core/update_checker'
require_relative '../DarkModeSKP/DarkModeSKP/ui/dialog'

class UpdateCheckerTest < Minitest::Test
  Checker = AG::DarkModeSKP::UpdateChecker
  Request = Sketchup::Http::Request

  def setup
    Checker.instance_variables.each { |variable| Checker.remove_instance_variable(variable) }
    Request.requests = []
    Request.fail_start = false
    Request.fail_create = false
    UI.timers = {}
    UI.timer_sequence = 0
    UI::Notification.notifications = []
  end

  def release(tag, **extra)
    { tag_name: tag, draft: false, prerelease: false }.merge(extra).to_json
  end

  def with_method_stub(target, method_name, replacement)
    original_method = target.method(method_name)
    implementation = replacement.is_a?(Proc) ? replacement : proc { |*_arguments, **_keywords| replacement }
    target.define_singleton_method(method_name, implementation)
    yield
  ensure
    target.define_singleton_method(method_name, original_method) if original_method
  end

  def test_async_check_reports_new_release_and_cleans_up
    states = []
    assert Checker.check { states << Checker.state[:status] }
    request = Request.requests.last
    assert_equal Checker::API_URL, request.url
    assert_equal 'https://api.github.com/repos/gordanog/Dark-Mode-SKP-Public/releases/latest', request.url
    assert_equal 'GET', request.method
    assert_equal "DarkModeSKP/#{AG::DarkModeSKP::VERSION}", request.headers['User-Agent']
    assert_equal ['checking'], states
    refute Checker.check
    assert_equal 1, Request.requests.length

    request.respond(200, release('v0.10.0'))
    assert_equal({ status: 'available', latest_version: '0.10.0' }, Checker.state)
    assert_equal ['checking', 'available'], states
    assert_empty UI.timers
    refute request.cancelled
  end

  def test_same_or_older_releases_do_not_offer_a_downgrade
    ['v0.1.9', 'v0.3.0', "v#{AG::DarkModeSKP::VERSION}"].each do |tag|
      Checker.check
      Request.requests.last.respond(200, release(tag))
      assert_equal 'current', Checker.state[:status]
    end
  end

  def test_versions_are_compared_numerically
    assert_equal 1, Checker.version_parts('v0.10.0') <=> Checker.version_parts('0.9.9')
    assert_equal [1, 2, 3], Checker.version_parts('v1.2.3+build.9')
    assert_equal [0, 3, 0], Checker.version_parts('0.3')
    assert_equal [0, 3, 0], Checker.version_parts('v0.3+build.9')
    assert_equal 0, Checker.version_parts('v0.3.0') <=> Checker.version_parts('0.3')
    assert_equal 1, Checker.version_parts('v0.4') <=> Checker.version_parts('0.3')
    assert_nil Checker.version_parts('v0.3-beta.1')
    assert_nil Checker.version_parts('v0.3.')
    assert_nil Checker.version_parts('v0')
    assert_nil Checker.version_parts('v1.2.3-beta.1')
    assert_nil Checker.version_parts('not-a-version')
  end

  def test_invalid_and_nonstable_responses_are_not_reported_as_up_to_date
    [release('bad'), release('v9.0.0', prerelease: true), release('v9.0.0', draft: true), 'null', '[]', '<html>error</html>'].each do |body|
      Checker.check
      Request.requests.last.respond(200, body)
      assert_equal 'error', Checker.state[:status]
      assert_empty UI.timers
    end
  end

  def test_http_failures_leave_the_checker_retryable
    [0, 403, 404, 429, 500].each do |status_code|
      assert Checker.check
      Request.requests.last.respond(status_code, '')
      assert_equal(status_code == 404 ? 'unavailable' : 'error', Checker.state[:status])
      assert_empty UI.timers
    end
  end

  def test_timeout_cancels_and_ignores_late_response
    Checker.check
    request = Request.requests.last
    UI.timers.values.last.call
    assert request.cancelled
    assert_equal 'error', Checker.state[:status]
    assert_empty UI.timers

    Checker.check
    retry_request = Request.requests.last
    request.respond(200, release('v9.0.0'))
    assert_equal 'checking', Checker.state[:status]
    retry_request.respond(200, release('v0.4.0'))
    assert_equal 'available', Checker.state[:status]
  end

  def test_failure_to_start_or_construct_a_request_is_handled
    Request.fail_start = true
    refute Checker.check
    assert Request.requests.last.cancelled
    assert_equal 'error', Checker.state[:status]
    assert_empty UI.timers

    Request.fail_start = false
    Request.fail_create = true
    refute Checker.check
    assert_equal 'error', Checker.state[:status]
    Request.fail_create = false
    assert Checker.check
  end

  class FakeDialog
    attr_reader :callbacks, :scripts

    def initialize
      @callbacks = {}
      @scripts = []
    end

    def add_action_callback(name, &block)
      @callbacks[name] = block
    end

    def execute_script(script)
      @scripts << script
    end
  end

  def boot_plugin(settings = {})
    plugin = AG::DarkModeSKP
    plugin.instance_variable_set(:@settings, AG::DarkModeSKP::DEFAULT_SETTINGS.merge(settings))
    plugin.instance_variable_set(:@dialog, nil)
    plugin.instance_variable_set(:@startup_applied, false)
    $dark_mode_test_loaded_files.clear
    entry_point = File.expand_path('../DarkModeSKP/DarkModeSKP/main.rb', __dir__)
    load entry_point
    plugin
  end

  def test_startup_notifies_about_new_release_without_opening_dialog
    plugin = boot_plugin
    assert_equal true, plugin.current_settings[:auto_check_updates]
    assert_nil plugin.instance_variable_get(:@dialog)
    assert_equal 1, UI.timers.length
    assert_empty Request.requests
    UI.timers.delete(UI.timers.keys.first).call
    assert_equal 1, Request.requests.length
    assert_empty UI::Notification.notifications
    with_method_stub(plugin, :logo_icon_path, 'notification-logo.svg') do
      Request.requests.last.respond(200, release('v0.4'))
    end
    notification = UI::Notification.notifications.fetch(0)
    assert_equal 1, UI::Notification.notifications.length
    assert_equal AG::DarkModeSKP::PLUGIN_NAME, notification.arguments[0].name
    assert_equal 'Dark Mode SKP is out of date. Version 0.4.0 is available.', notification.arguments[1]
    assert_equal ['notification-logo.svg', AG::DarkModeSKP::PLUGIN_NAME], notification.arguments.drop(2)
    assert_equal 'Close', notification.accept_label
    assert notification.shown
    assert_nil plugin.instance_variable_get(:@dialog)
    assert_empty UI.timers

    load File.expand_path('../DarkModeSKP/DarkModeSKP/main.rb', __dir__)
    assert_empty UI.timers
    assert_equal 1, Request.requests.length
    assert_equal 1, UI::Notification.notifications.length
  end

  def test_disabled_startup_check_does_not_make_requests
    plugin = boot_plugin(auto_check_updates: false)
    assert_empty UI.timers
    plugin.check_for_updates
    assert_empty Request.requests
    assert_empty UI::Notification.notifications
  end

  def test_opt_out_before_startup_timer_or_during_request_is_respected
    [false, true].each do |request_started|
      plugin = boot_plugin
      startup = UI.timers.delete(UI.timers.keys.first)
      startup.call if request_started
      with_method_stub(AG::DarkModeSKP::GlobalSettings, :save, true) do
        plugin.update_settings(auto_check_updates: false)
      end
      if request_started
        Request.requests.last.respond(200, release('v0.4.0'))
      else
        startup.call
        assert_empty Request.requests
      end
      assert_empty UI::Notification.notifications
      assert_empty UI.timers
    end
  end

  def test_current_missing_and_failed_releases_do_not_notify
    [[200, release("v#{AG::DarkModeSKP::VERSION}")], [200, release('v0.1.0')], [404, ''], [500, ''], [200, 'invalid']].each do |status_code, body|
      boot_plugin
      UI.timers.delete(UI.timers.keys.first).call
      Request.requests.last.respond(status_code, body)
      assert_empty UI::Notification.notifications
      assert_empty UI.timers
    end
  end

  def test_update_preference_is_persisted_and_defaults_to_enabled
    settings = AG::DarkModeSKP::GlobalSettings
    defaults = AG::DarkModeSKP::DEFAULT_SETTINGS
    with_method_stub(settings, :settings_path, 'unused-settings-path') do
      with_method_stub(File, :exist?, true) do
        with_method_stub(File, :read, '{}') do
          assert_equal true, settings.merge_with_defaults(defaults)[:auto_check_updates]
        end
        with_method_stub(File, :read, '{"auto_check_updates":false}') do
          assert_equal false, settings.merge_with_defaults(defaults)[:auto_check_updates]
        end
      end
      written = nil
      with_method_stub(File, :write, ->(_path, contents, **_options) { written = contents }) do
        assert settings.save(defaults.merge(auto_check_updates: false))
      end
      assert_equal false, JSON.parse(written).fetch('auto_check_updates')
    end
  end

  def test_dialog_callback_persists_preference_without_manual_update_controls
    plugin = boot_plugin(auto_check_updates: false)
    dialog = FakeDialog.new
    plugin.instance_variable_set(:@dialog, dialog)
    plugin.setup_callbacks
    refute dialog.callbacks.key?('checkForUpdates')
    refute dialog.callbacks.key?('openReleaseRepository')
    refute plugin.dialog_state.key?(:update)
    saved = nil
    with_method_stub(AG::DarkModeSKP::GlobalSettings, :save, ->(settings) { saved = settings.dup; true }) do
      dialog.callbacks.fetch('updateSettings').call(nil, '{"auto_check_updates":true}')
    end
    assert_equal true, saved.fetch(:auto_check_updates)
    assert_equal true, plugin.current_settings[:auto_check_updates]
    assert_includes dialog.scripts.last, '"auto_check_updates":true'
    assert_empty Request.requests
    assert_empty UI.timers
  ensure
    plugin.instance_variable_set(:@dialog, nil) if plugin
  end

  def test_generated_dialog_javascript_states
    html = AG::DarkModeSKP::Dialog.generate_html(AG::DarkModeSKP::DEFAULT_SETTINGS)
    assert_includes html, "<label>v#{AG::DarkModeSKP::VERSION}</label>"
    disabled_html = AG::DarkModeSKP::Dialog.generate_html(AG::DarkModeSKP::DEFAULT_SETTINGS.merge(auto_check_updates: false))
    refute_match(/id="autoCheckUpdates"[^>]*\bchecked\s/, disabled_html)
    script = <<~'JAVASCRIPT'
      const assert = require('node:assert/strict');
      const vm = require('node:vm');
      const html = require('node:fs').readFileSync(0, 'utf8');
      const source = html.match(/<script>([\s\S]*?)<\/script>/)[1];
      assert.doesNotMatch(html, /checkForUpdatesBtn|updateRepositoryLink|refreshUpdateState|update-status|version-heading/);
      assert.match(html, /<h2>Version<\/h2>/);
      assert.match(html, /id="autoCheckUpdates"[^>]*\bchecked\s/);
      assert.match(html, /<label for="autoCheckUpdates">Auto Check For Updates<\/label>/);
      assert.match(html, /id="autoCheckUpdates"[^>]*>[\s\S]*?<div class="info-icon" style="margin-left: 8px;">\s*\?\s*<div class="info-tooltip">Checks for a newer release when SketchUp starts and shows a notification if an update is available\. Updates are not installed automatically\.<\/div>/);
      const settingsMarkup = html.split('<h2>Global Settings</h2>')[1];
      let settingsDepth = 1;
      let foundToggle = false;
      for (const [tag] of settingsMarkup.matchAll(/<\/?div\b[^>]*>|<input\b[^>]*>/g)) {
        if (tag.startsWith('</div')) settingsDepth--;
        else if (tag.startsWith('<div')) settingsDepth++;
        assert.ok(settingsDepth > 0, 'Startup toggle must be inside Global Settings.');
        if (tag.includes('id="autoCheckUpdates"')) { foundToggle = true; break; }
      }
      assert.ok(foundToggle);
      const classList = { add() {}, remove() {}, toggle() {} };
      const elements = {};
      for (const name of ['autoCheckUpdates', 'darkModeStatus', 'darkModeEnabled', 'applyDarkStyle']) {
        elements[name] = { hidden: false, disabled: false, textContent: '', classList };
      }
      const savedSettings = [];
      let dialogReady = 0;
      let resizes = 0;
      const context = {
        document: {
          readyState: 'loading',
          addEventListener() {},
          getElementById(name) { return elements[name]; },
          body: { classList },
          documentElement: { style: {} }
        },
        window: { requestAnimationFrame() { resizes++; } },
        setTimeout() { return 1; },
        clearTimeout() {},
        sketchup: {
          dialogReady() { dialogReady++; },
          updateSettings(settings) { savedSettings.push(JSON.parse(settings)); }
        }
      };
      vm.createContext(context);
      vm.runInContext(source, context);
      context.initializeDialog();
      const toggle = elements.autoCheckUpdates;
      assert.equal(dialogReady, 1);
      assert.equal(typeof context.checkForUpdates, 'undefined');
      assert.equal(typeof context.openReleaseRepository, 'undefined');

      context.refreshDialogState({
        dark_mode_applied: true,
        dark_mode_enabled: true,
        settings: { apply_dark_style: true, auto_check_updates: true }
      });
      assert.equal(toggle.checked, true);
      assert.equal(toggle.disabled, false);
      assert.equal(elements.darkModeEnabled.checked, true);
      assert.equal(elements.applyDarkStyle.checked, true);

      context.updateSetting('auto_check_updates', false);
      assert.deepEqual(savedSettings, [{ auto_check_updates: false }]);
      context.refreshDialogState({ settings: { auto_check_updates: false } });
      assert.equal(toggle.checked, false);
      assert.equal(toggle.disabled, false);
      context.refreshDialogState({ settings: {} });
      assert.equal(toggle.checked, true);
      context.refreshDialogState({ environment_supported: false, settings: { auto_check_updates: true } });
      assert.equal(toggle.disabled, true);
      assert.ok(resizes > 0);
      process.stdout.write('Dialog state checks passed.');
    JAVASCRIPT
    output, errors, result = Open3.capture3({ 'ELECTRON_RUN_AS_NODE' => '1' },
      ENV.fetch('DARKMODE_TEST_NODE', 'node'), '-e', script, stdin_data: html)
    assert result.success?, "#{output}\n#{errors}"
    assert_includes output, 'Dialog state checks passed.'
  rescue Errno::ENOENT
    skip 'Set DARKMODE_TEST_NODE to a Node.js executable to test the dialog JavaScript.'
  end
end