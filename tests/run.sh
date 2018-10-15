#!/usr/bin/env bash
source $(dirname $0)/helpers.sh

# Create and start services
pg_ctlcluster $1 my_cluster start
redis-server --daemonize yes

# Setup Redis
redis-cli < tests/redis.commands

# Setup Postgres
exec_psql /holycorn/tests/setup.sql
exec_psql /holycorn/tests/setups/$1.sql
exec_psql /holycorn/tests/run.sql

# Execute tests
expected_output=$(cat tests/expected_outputs/$1)
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
