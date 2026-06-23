#!/bin/bash
rm -rf logs
mkdir logs
grep container_name docker-compose.yaml  | grep -v "#" | sed s/container_name:// | sed s/\"//g | while read -r line ; do
   docker logs $line > logs/$line.log 2>&1
done

tar -cvzf l2plusLogs-$(date +%F-%H%M%S).tgz logs
