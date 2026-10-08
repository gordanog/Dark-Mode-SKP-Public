# DarkModeSKP - HtmlDialog HTML generator

module AG
module DarkModeSKP
module Dialog

  def self.generate_html(settings, state = {})
    environment_supported = state.fetch(:environment_supported, true)
    environment_supported = environment_supported == true || environment_supported.to_s == 'true'
    dark_mode_enabled = state.fetch(:dark_mode_enabled, settings.fetch(:dark_mode_enabled, false))
    dark_mode_enabled = dark_mode_enabled == true || dark_mode_enabled.to_s == 'true'
    dark_mode_applied = state.fetch(:dark_mode_applied, dark_mode_enabled)
    dark_mode_applied = dark_mode_applied == true || dark_mode_applied.to_s == 'true'
    restart_required = state.fetch(:restart_required, dark_mode_enabled != dark_mode_applied)
    restart_required = restart_required == true || restart_required.to_s == 'true'
    apply_dark_style = settings.fetch(:apply_dark_style, false) == true || settings.fetch(:apply_dark_style, false).to_s == 'true'
    body_class = environment_supported && dark_mode_applied ? 'dark-mode' : ''
    status_class = if !environment_supported
      'is-unsupported'
    elsif restart_required
      'is-restart-required'
    elsif dark_mode_applied
      'is-enabled'
    else
      'is-disabled'
    end
    status_text = if !environment_supported
      escape_html(state.fetch(:unsupported_message, UNSUPPORTED_ENVIRONMENT_MESSAGE))
    elsif restart_required
      "SketchUp restart required to #{dark_mode_enabled ? 'enable' : 'disable'}"
    elsif dark_mode_applied
      'Enabled'
    else
      'Disabled'
    end
    toggle_disabled = environment_supported ? '' : 'disabled'
    initial_theme_enabled = environment_supported && dark_mode_applied
    version = escape_html(VERSION)

    <<~HTML
      <!DOCTYPE html>
      <html>
      <head>
        <meta charset="UTF-8">
        <style>
          :root {
            --bg-primary: #fafafa;
            --bg-secondary: #ffffff;
            --bg-hover: #f5f5f5;
            --text-primary: #333333;
            --text-secondary: #666666;
            --text-disabled: #999999;
            --border-primary: #e0e0e0;
            --border-light: #f0f0f0;
            --button-primary: #4a90e2;
            --button-hover: #357abd;
            --button-disabled: #cccccc;
            --button-text: #ffffff;
            --focus-color: #4a90e2;
            --input-bg: #ffffff;
            --input-border: #e0e0e0;
            --success: #28a745;
            --warning: #ffc107;
            --error: #dc3545;
            --info: #17a2b8;
            --shadow: rgba(0, 0, 0, 0.1);
            --shadow-strong: rgba(0, 0, 0, 0.15);
          }

          body.dark-mode {
            --bg-primary: #1e1e1e;
            --bg-secondary: #2d2d2d;
            --bg-hover: #3a3a3a;
            --text-primary: #e0e0e0;
            --text-secondary: #b0b0b0;
            --text-disabled: #666666;
            --border-primary: #404040;
            --border-light: #353535;
            --button-primary: #4a90e2;
            --button-hover: #5a9fd4;
            --button-disabled: #555555;
            --button-text: #ffffff;
            --focus-color: #5a9fd4;
            --input-bg: #2d2d2d;
            --input-border: #404040;
            --success: #3dbd5d;
            --warning: #f0ad4e;
            --error: #e85d6b;
            --info: #5bc0de;
            --shadow: rgba(0, 0, 0, 0.3);
            --shadow-strong: rgba(0, 0, 0, 0.5);
          }

          body.theme-switching *:not(.toggle-slider):not(.toggle-slider::before),
          body.theme-switching *::after {
            transition: none !important;
          }

          html {
            overflow-y: hidden;
          }

          body {
            font-family: Arial, sans-serif;
            margin: 0;
            padding: 0;
            background: var(--bg-primary);
            color: var(--text-primary);
            font-size: 11px;
          }

          .sticky-header {
            position: sticky;
            top: 0;
            z-index: 1000;
            background: var(--bg-secondary);
            padding: 12px;
            box-shadow: 0 2px 6px var(--shadow);
            border-bottom: 1px solid var(--border-primary);
          }

          .status-messages {
            position: fixed;
            top: 15%;
            left: 50%;
            transform: translateX(-50%);
            z-index: 20000;
            width: 80%;
            pointer-events: none;
          }

          .status-message {
            padding: 12px 16px;
            margin: 8px 0;
            border-radius: 8px;
            font-size: 12px;
            display: none;
            align-items: center;
            box-shadow: 0 4px 12px var(--shadow-strong);
            pointer-events: auto;
            background: var(--bg-secondary);
            color: var(--text-primary);
            border-left: 4px solid var(--border-primary);
            width: 100%;
            box-sizing: border-box;
          }

          .status-message.show { display: flex; }
          .status-message.warning { border-left-color: var(--warning); }
          .status-message.error { border-left-color: var(--error); }
          .status-message.info { border-left-color: var(--info); }
          .status-message.success { border-left-color: var(--success); }
          .status-message-text { flex: 1; }

          .status-message-close {
            background: none;
            border: none;
            color: inherit;
            cursor: pointer;
            padding: 0 4px;
            font-size: 16px;
            opacity: 0.7;
          }

          .status-message-close:hover {
            opacity: 1;
            background: none;
          }

          .container {
            max-width: 420px;
            margin: 0 auto;
            padding: 12px;
          }

          .section {
            background: var(--bg-secondary);
            padding: 0 0 5px 0;
            margin-bottom: 12px;
            border-radius: 10px;
            border: 1px solid var(--border-primary);
            overflow: hidden;
          }

          .container > .section:last-child {
            margin-bottom: 0;
          }

          h2 {
            margin-top: 0;
            margin-bottom: 5px;
            font-size: 12px;
            color: var(--text-primary);
            border-bottom: 1px solid var(--border-primary);
            padding: 8px 10px 6px 8px;
            background: var(--bg-hover);
            border-radius: 0;
            font-weight: bold;
          }

          .section > .input-group,
          .section > .inline-group,
          .section > table,
          .section > button,
          .section > div:not(h2) {
            background: var(--bg-secondary);
            padding: 5px 12px;
          }

          .section > div > .input-group {
            background: transparent;
            padding: 0;
          }

          button {
            background: var(--button-primary);
            color: var(--button-text);
            border: none;
            padding: 6px 12px;
            border-radius: 4px;
            cursor: pointer;
            font-size: 11px;
            transition: background 0.2s;
          }

          button:hover,
          button:active {
            background: var(--button-hover);
          }

          button:disabled,
          button.disabled {
            background: var(--bg-hover);
            color: var(--text-disabled);
            cursor: not-allowed;
            opacity: 0.5;
          }

          button:disabled:hover,
          button.disabled:hover {
            background: var(--bg-hover);
          }

          input:disabled,
          select:disabled {
            background: var(--bg-hover);
            color: var(--text-disabled);
            cursor: not-allowed;
            opacity: 0.6;
          }

          .input-group {
            margin-bottom: 0;
            display: flex;
            flex-wrap: wrap;
            align-items: center;
            gap: 8px;
            justify-content: space-between;
          }

          .input-group > label {
            order: 1;
            flex: 0 0 auto;
            text-align: left;
          }

          .input-group > .info-text {
            flex-basis: 100%;
            order: 3;
            margin-top: 0;
            margin-bottom: 0;
            padding-left: 0;
            text-align: left;
          }

          label {
            display: inline;
            margin-bottom: 0;
            font-weight: 500;
            color: var(--text-secondary);
            font-size: 11px;
            white-space: nowrap;
          }

          .info-text {
            font-size: 10px;
            color: var(--text-secondary);
            margin-top: 3px;
            margin-bottom: 0;
          }

          .info-icon {
            display: inline-flex;
            align-items: center;
            justify-content: center;
            width: 16px;
            height: 16px;
            border-radius: 50%;
            background: var(--bg-hover);
            color: var(--text-secondary);
            font-size: 11px;
            font-weight: 600;
            cursor: help;
            margin-left: 4px;
            flex-shrink: 0;
            position: relative;
          }

          .info-icon:hover {
            background: var(--border-primary);
          }

          .info-tooltip {
            display: none;
            position: absolute;
            bottom: 100%;
            right: 0;
            transform: translateY(-8px);
            background: var(--text-primary);
            color: var(--bg-primary);
            padding: 6px 8px;
            border-radius: 4px;
            font-size: 10px;
            white-space: normal;
            width: max-content;
            max-width: min(280px, 75vw);
            word-wrap: break-word;
            z-index: 1000;
            pointer-events: none;
            box-shadow: 0 2px 8px var(--shadow);
          }

          .info-icon:hover .info-tooltip {
            display: block;
          }

          .info-icon .info-tooltip::after {
            content: '';
            position: absolute;
            top: 100%;
            right: 4px;
            border: 4px solid transparent;
            border-top-color: var(--text-primary);
          }

          .toggle-switch {
            position: relative;
            display: inline-block;
            width: 32px;
            height: 18px;
            margin: 0 0 0 8px;
          }

          .toggle-switch input {
            opacity: 0;
            width: 0;
            height: 0;
          }

          .toggle-slider {
            position: absolute;
            cursor: pointer;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background-color: var(--bg-hover);
            transition: background-color 0.3s ease;
            border-radius: 18px;
            border: 1px solid var(--border-primary);
          }

          .toggle-slider:before {
            position: absolute;
            content: "";
            height: 12px;
            width: 12px;
            left: 2px;
            bottom: 2px;
            background-color: var(--text-disabled);
            transition: transform 0.3s ease, background-color 0.3s ease;
            border-radius: 50%;
          }

          .toggle-switch input:checked + .toggle-slider {
            background-color: var(--button-primary);
          }

          .toggle-switch input:checked + .toggle-slider:before {
            transform: translateX(14px);
            background-color: white;
          }

          .toggle-switch input:disabled + .toggle-slider {
            opacity: 0.5;
            cursor: not-allowed;
          }

          .toggle-switch input:disabled + .toggle-slider:before {
            background-color: var(--text-disabled);
          }

          .dialog-header {
            display: flex;
            align-items: center;
            justify-content: space-between;
            width: 100%;
            min-height: 32px;
          }

          .header-status-group {
            border: 1px solid var(--border-primary);
            border-radius: 8px;
            padding: 0;
            background: var(--bg-hover);
            display: flex;
            align-items: stretch;
            flex-wrap: nowrap;
            width: fit-content;
            box-sizing: border-box;
          }

          .dialog-status {
            display: inline-flex;
            align-items: center;
            gap: 6px;
            padding: 6px 10px;
            color: var(--text-primary);
            font-size: 12px;
            font-weight: bold;
          }

          .dialog-status::before {
            content: '';
            width: 8px;
            height: 8px;
            border-radius: 50%;
            background: var(--error);
          }

          .dialog-status.is-unsupported::before {
            background: var(--error);
          }

          .dialog-status.is-enabled::before {
            background: var(--success);
          }

          .dialog-status.is-applying::before {
            background: var(--warning);
          }

          .dialog-status.is-restart-required::before {
            background: var(--warning);
          }
        </style>
      </head>
      <body class="#{body_class}">
        <div id="dialog-content">
        <div class="sticky-header">
          <div class="dialog-header">
            <div class="header-status-group">
              <span id="darkModeStatus" class="dialog-status #{status_class}">#{status_text}</span>
            </div>
            <label class="toggle-switch" style="margin: 0;">
              <input type="checkbox" id="darkModeEnabled" #{dark_mode_enabled ? 'checked' : ''} #{toggle_disabled} onchange="toggleSketchUpDarkMode(this.checked)">
              <span class="toggle-slider"></span>
            </label>
          </div>
        </div>

        <div class="status-messages">
          <div id="status-message" class="status-message">
            <span class="status-message-text" id="status-text"></span>
            <button class="status-message-close" onclick="hideStatusMessage()">&times;</button>
          </div>
        </div>

        <div class="container">
          <div class="section">
            <h2>Global Settings</h2>
            <div class="input-group">
              <div style="display: flex; align-items: center; flex: 1; justify-content: space-between;">
                <label>Apply Dark Style</label>
                <div style="display: flex; align-items: center;">
                  <label class="toggle-switch">
                    <input type="checkbox" id="applyDarkStyle" #{apply_dark_style ? 'checked' : ''} #{toggle_disabled} onchange="updateSetting('apply_dark_style', this.checked)">
                    <span class="toggle-slider"></span>
                  </label>
                  <div class="info-icon" style="margin-left: 8px;">
                    ?
                    <div class="info-tooltip">When Dark Mode SKP is enabled, applies a dark style in SketchUp automatically.</div>
                  </div>
                </div>
              </div>
            </div>
          </div>

          <div class="section">
            <h2>Version</h2>
            <div class="input-group">
              <label>v#{version}</label>
            </div>
          </div>
        </div>
        </div>

        <script>
          function updateSetting(key, value) {
            var settings = {};
            settings[key] = !!value;
            sketchup.updateSettings(JSON.stringify(settings));
          }

          function applyDialogTheme(enabled) {
            document.body.classList.add('theme-switching');
            document.body.classList.toggle('dark-mode', !!enabled);
            document.documentElement.style.colorScheme = enabled ? 'dark' : 'light';
            setTimeout(function() {
              document.body.classList.remove('theme-switching');
            }, 50);
          }

          function toggleSketchUpDarkMode(enabled) {
            setDarkModeStatus('restart-required', enabled);
            sketchup.setDarkMode(!!enabled);
          }

          function setDarkModeStatus(statusName, darkModeEnabled, unsupportedMessage) {
            var status = document.getElementById('darkModeStatus');
            if (!status) return;

            status.classList.remove('is-enabled', 'is-disabled', 'is-applying', 'is-restart-required', 'is-unsupported');
            if (statusName === 'unsupported') {
              status.textContent = unsupportedMessage || 'Supports only SketchUp 2024+ on Windows';
              status.classList.add('is-unsupported');
              return;
            }

            if (statusName === 'restart-required') {
              status.textContent = darkModeEnabled ? 'SketchUp restart required to enable' : 'SketchUp restart required to disable';
              status.classList.add('is-restart-required');
              return;
            }

            var enabled = statusName === 'enabled';
            status.textContent = enabled ? 'Enabled' : 'Disabled';
            status.classList.add(enabled ? 'is-enabled' : 'is-disabled');
          }

          function refreshDialogState(state) {
            state = state || {};
            var settings = state.settings || {};
            var environmentSupported = state.environment_supported !== false;
            var darkToggle = document.getElementById('darkModeEnabled');
            if (darkToggle) {
              darkToggle.checked = !!state.dark_mode_enabled;
              darkToggle.disabled = !environmentSupported;
            }
            setDarkModeStatus(
              environmentSupported ? (state.restart_required ? 'restart-required' : state.dark_mode_applied ? 'enabled' : 'disabled') : 'unsupported',
              state.dark_mode_enabled,
              state.unsupported_message
            );
            applyDialogTheme(environmentSupported && !!state.dark_mode_applied);

            var darkStyleToggle = document.getElementById('applyDarkStyle');
            if (darkStyleToggle) {
              darkStyleToggle.checked = !!settings.apply_dark_style;
              darkStyleToggle.disabled = !environmentSupported;
            }
          }

          var statusMessageTimeout = null;

          function showStatusMessage(text, type) {
            var messageDiv = document.getElementById('status-message');
            var textSpan = document.getElementById('status-text');
            if (!messageDiv || !textSpan) return;

            if (statusMessageTimeout) clearTimeout(statusMessageTimeout);
            textSpan.textContent = text;
            messageDiv.classList.remove('warning', 'error', 'info', 'success');
            messageDiv.classList.add(type || 'warning');
            messageDiv.classList.add('show');
            statusMessageTimeout = setTimeout(hideStatusMessage, 5000);
          }

          function hideStatusMessage() {
            var messageDiv = document.getElementById('status-message');
            if (messageDiv) messageDiv.classList.remove('show');
            if (statusMessageTimeout) {
              clearTimeout(statusMessageTimeout);
              statusMessageTimeout = null;
            }
          }

          function sizeDialogToContent() {
            if (typeof sketchup === 'undefined' || !sketchup.resizeDialog) return;

            var content = document.getElementById('dialog-content');
            if (!content) return;

            var contentHeight = Math.ceil(content.getBoundingClientRect().height);
            var viewportHeight = Math.ceil(window.innerHeight);
            sketchup.resizeDialog(contentHeight, viewportHeight);
          }

          function initializeDialog() {
            applyDialogTheme(#{initial_theme_enabled ? 'true' : 'false'});
            if (typeof sketchup !== 'undefined' && sketchup.dialogReady) {
              sketchup.dialogReady();
            }
            window.requestAnimationFrame(sizeDialogToContent);
          }

          if (document.readyState === 'loading') {
            document.addEventListener('DOMContentLoaded', initializeDialog);
          } else {
            initializeDialog();
          }
        </script>
      </body>
      </html>
    HTML
  end

  def self.escape_html(value)
    value.to_s
      .gsub('&', '&amp;')
      .gsub('<', '&lt;')
      .gsub('>', '&gt;')
      .gsub('"', '&quot;')
      .gsub("'", '&#39;')
  end

end
end
end