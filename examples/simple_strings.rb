class Producer
  def initialize(env = {}) # env contains informations provided by Holycorn
  end

  def each
    @enum ||= Enumerator.new do |y|
      10.times do |t|
        y.yield [ "Hello #{t}" ]
      end
    end
    @enum.next
  end
  self
end
