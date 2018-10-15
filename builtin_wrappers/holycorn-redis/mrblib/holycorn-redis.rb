# Known limitations
#
# 1. We should use SCAN instead of KEYS (PR accepted :-D)
# 2. Hashes, Sets and Sorted Sets are not supported yet (ditto)
class HolycornRedis
  def initialize(env = {})
    host = env.fetch('host') { raise ArgumentError, 'host not provided' }
    port = env.fetch('port') { raise ArgumentError, 'port not provided' }
    db   = env.fetch('db')   { raise ArgumentError, 'db not provided' }

    @r = Redis.new host, port.to_i
    @r.select db.to_i
    @values = (@r.keys('*') || []).to_enum
  end

  def each
    if (val = @values.next)
      [val.to_s, @r.get(val).to_s]
    else
      nil
    end
  end

  def self.import_schema(args)
<<-SCHEMA
	CREATE FOREIGN TABLE #{args['local_schema']}.#{args['prefix']}redis_table
        ( key text
        , value text
        )
	SERVER #{args['server_name']}
	OPTIONS ( wrapper_class 'HolycornRedis'
	        , host '#{args['host']}'
	        , port '#{args['port']}'
	        , db '#{args['db']}'
	);
SCHEMA
  end
  self
end
