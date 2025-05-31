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
  SAMPLES_PER_SIGNAL = 50

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
          # according to `cat CDD_PATH_ON_RASPBERRY_PI` expecting binary string i.e.: "0101010101011100101010101001" 
          raw_signals_data = \
            Net::SSH.start(ENV['RASPBERRY_PI_IP'], ENV['RASPBERRY_PI_HOST_NAME'], password: ENV['RASPBERRY_PI_PASSWORD']) do |ssh|
              ssh.exec!("cat #{CDD_PATH_ON_RASPBERRY_PI}")
            end
          @signals_data = [raw_signals_data.chars[0...SAMPLES_PER_SIGNAL].map {|c| c.to_i}, raw_signals_data.chars[SAMPLES_PER_SIGNAL..-1].map {|c| c.to_i}]
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
