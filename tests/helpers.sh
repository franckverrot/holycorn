PSQL="psql -p 5433"
function exec_psql {
  su -c "$PSQL -q < $1" - postgres
}

