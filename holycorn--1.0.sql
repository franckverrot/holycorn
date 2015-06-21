\echo Use "CREATE EXTENSION holycorn" to load this file. \quit

CREATE FUNCTION holycorn_handler()
RETURNS fdw_handler
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE FUNCTION holycorn_validator(text[], oid)
RETURNS void
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE FOREIGN DATA WRAPPER holycorn
  HANDLER holycorn_handler
  VALIDATOR holycorn_validator;
