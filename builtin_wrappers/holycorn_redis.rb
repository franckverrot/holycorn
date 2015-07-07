class HolycornRedis
  def initialize(env = {})
    host = env.fetch('host', '127.0.0.1')
    port = env.fetch('port', '6379').to_i
    db   = env.fetch('db',   '0').to_i

    @r = Redis.new host, por
    @r.select db
    @values = @r.keys('*').to_enum
  end

  def each
    if (val = @values.next)
      [val.to_s]
    else
      nil
    end
  end
  self
end
