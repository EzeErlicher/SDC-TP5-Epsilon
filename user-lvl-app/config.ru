# external libraries
require 'net/ssh'

# core libraries
require 'json'

# src code
require_relative 'env'

# provide public access to scripts and styles folders
use Rack::Static, urls: ["/scripts", "/styles"]

# HTTP Server.
class Webserver

  DEFAULT_RESPONSE = [404, {}, []].freeze
  HTTP_HEADER_TEMPLATE = {
    'Content-Type' => 'text/html; charset=UTF-8',
    'Date' => Time.now.httpdate,
    'Server' => 'Puma'
  }.freeze
  CDD_PATH_ON_RASPBERRY_PI = '/dev/signals_reader'.freeze

  # @param env [Hash<String, String>]
  # @return [Array(Integer, Hash<String, String>, Array<String>)]
  def call(env)
    route(Rack::Request.new(env))
  end

  private

  # @param req [Rack::Request]
  # @return [Array(Integer, Hash<String, String>, Array<String>)]
  def route(req)
    if((@req = req).get?)
      case(req.path)
        when('/')
          # TODO: Uncomment on production
=begin
          # expecting i.e.: "[[v_0, v_1, ..., v_49], [w_0, w_1, ..., v_49]]"
          raw_signals_data = \
            Net::SSH.start(ENV['RASPBERRY_PI_IP'], ENV['RASPBERRY_PI_HOST_NAME'], password: ENV['RASPBERRY_PI_PASSWORD']) do |ssh|
              ssh.exec!("cat #{CDD_PATH_ON_RASPBERRY_PI}")
            end
          @signals_data = eval(raw_signals_data)
=end
          # WARNING: Mock values
          @signals_data = [[0, 2, 4, 6, 8, 10, 12, 14, 16, 14, 12, 10, 8, 6, 4, 2, 0], [8, 5, 1, 5, 8, 11, 14, 11, 8, 5, 1, 5, 8, 11, 14, 11, 8]]
          [200, HTTP_HEADER_TEMPLATE, [render('main')]]
        when('/alive')
          # alive check, test/debugging purpose
          [200, {'Content-Type' => 'text/plain'}, ['Alive']]
        else
          DEFAULT_RESPONSE
      end
    else
      DEFAULT_RESPONSE
    end
  end

  # @param view [String]
  # @param req [NilClass, Rack::Request]
  def render(view)
    # render bottom to top
    @style = view if File.exist?("styles/#{view}.css")
    @script = view if File.exist?("scripts/#{view}.js")
    @content = render_template(view)
    render_template("_layout")
  end

  # @param template [String]
  # @param req [NilClass, Rack::Request]
  def render_template(template)
    template = File.open("./views/#{template}.html.erb") {|f| f.read}
    erb = ERB.new(template)
    erb.result(binding())
  end
end

=begin
RUNTIME
=end
run Webserver.new()
