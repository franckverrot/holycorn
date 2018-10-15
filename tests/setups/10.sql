IMPORT FOREIGN SCHEMA holycorn_schema
FROM SERVER holycorn_server
INTO holycorn_tables
OPTIONS ( wrapper_class 'HolycornRedis'
        , host '127.0.0.1'
        , port '6379'
        , db '0'
        , prefix 'holycorn_'
        );
