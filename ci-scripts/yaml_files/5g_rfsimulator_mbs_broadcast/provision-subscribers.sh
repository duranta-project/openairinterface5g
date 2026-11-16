#!/bin/bash
# Run once by the mongo image when the database is empty.
set -e
mongoimport --db=open5gs --collection=subscribers --type=json --jsonArray \
            --file=/docker-entrypoint-initdb.d/subscribers.json
