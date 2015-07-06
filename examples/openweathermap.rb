class OpenWeatherMap
  def initialize(env = {})
    @url = "http://api.openweathermap.org/data/2.5/forecast/daily?q=Lyon&mode=json&units=metric&cnt=7"

    http = HttpRequest.new()
    body = http.get(@url).body
    @results = JSON.parse(body)['list'].to_enum
  end

  def each
    if record = @results.next
      record.values_at('deg','speed').map(&:to_s)
    else
      nil
    end
  end
  self
end
