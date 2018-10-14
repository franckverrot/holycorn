SELECT left(current_setting('server_version_num'), 3) as server_version_num;
SELECT
   key
 , value
FROM
  redis_table
ORDER BY
  key
