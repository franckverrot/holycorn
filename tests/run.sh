#!/usr/bin/env bash
source $(dirname $0)/helpers.sh

# Create and start services
pg_ctlcluster 9.5 my_cluster start
redis-server --daemonize yes

# Setup Redis
redis-cli < tests/fixtures/redis.commands

# Setup Postgres
exec_psql /holycorn/tests/setup.sql
exec_psql /holycorn/tests/run.sql

# Execute tests
expected_output=$(cat tests/fixtures/expected_output)
actual_output=$(su -c "$PSQL -txqAF, -R\; < /holycorn/tests/run.sql" - postgres)

# Assert results
if [ "$expected_output" == "$actual_output" ]
then
  echo "OK"
  exit 0
else
  echo "KO, expected:"
  echo "$expected_output"
  echo "got:"
  echo "$actual_output"
  exit 1
fi
