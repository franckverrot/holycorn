class RedisFDW
  def initialize(env = {})
    @count = 0

    @r = Redis.new "127.0.0.1", 6379
    @r.select 0
  end

  def each
    if (val = @r.randomkey) && (@count < 100)
      @count += 1
      [val]
    else
      nil
    end
  end
  self
end
