CREATE FOREIGN TABLE holycorn_tables.holycorn_redis_table
  ( key text
  , value text
  )
  SERVER holycorn_server
  OPTIONS ( wrapper_class 'HolycornRedis'
          , host '127.0.0.1'
          , port '6379'
          , db '0'
  );
