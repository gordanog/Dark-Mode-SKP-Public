require 'json'

module AG
module DarkModeSKP
module UpdateChecker

  API_URL = 'https://api.github.com/repos/gordanog/Dark-Mode-SKP-Public/releases/latest'.freeze
  TIMEOUT_SECONDS = 15

  def self.state
    (@state || { status: 'idle' }).dup
  end

  def self.check(&on_change)
    return false if @request

    @on_change = on_change
    @state = { status: 'checking' }
    notify
    request = Sketchup::Http::Request.new(API_URL, Sketchup::Http::GET)
    @request = request
    request.headers = {
      'Accept' => 'application/vnd.github+json',
      'User-Agent' => "DarkModeSKP/#{VERSION}",
      'X-GitHub-Api-Version' => '2022-11-28'
    }
    @timeout_id = UI.start_timer(TIMEOUT_SECONDS, false) do
      finish(request, error_state('Update check timed out. Please try again.'), cancel: true)
    end
    started = request.start do |_completed_request, response|
      next unless @request.equal?(request)

      begin
        finish(request, response_state(response))
      rescue StandardError
        finish(request, error_state)
      end
    end
    finish(request, error_state, cancel: true) unless started
    !!started
  rescue StandardError
    if request && @request.equal?(request)
      finish(request, error_state, cancel: true)
    else
      @state = error_state
      notify
    end
    false
  end

  def self.version_parts(version)
    match = /\Av?(\d+)\.(\d+)(?:\.(\d+))?(?:\+[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?\z/.match(version.to_s)
    match && match.captures.map { |part| (part || '0').to_i }
  end

  def self.response_state(response)
    case response.status_code
    when 200
      release = JSON.parse(response.body)
      return error_state unless release.is_a?(Hash)
      return error_state if release['draft'] || release['prerelease']

      latest = version_parts(release['tag_name'])
      installed = version_parts(VERSION)
      return error_state unless latest && installed

      {
        status: (latest <=> installed) == 1 ? 'available' : 'current',
        latest_version: latest.join('.')
      }
    when 404
      { status: 'unavailable', message: 'No published release was found.' }
    when 403, 429
      error_state('GitHub could not complete the check. Please try again later.')
    else
      error_state
    end
  end

  def self.error_state(message = 'Could not check for updates. Please try again.')
    { status: 'error', message: message }
  end

  def self.finish(request, result, cancel: false)
    return unless @request.equal?(request)

    @request = nil
    timeout_id = @timeout_id
    @timeout_id = nil
    UI.stop_timer(timeout_id) if timeout_id
    begin
      request.cancel if cancel
    rescue StandardError
      nil
    end
    @state = result
    notify
  end

  def self.notify
    @on_change.call if @on_change
  rescue StandardError
    nil
  end

end
end
end