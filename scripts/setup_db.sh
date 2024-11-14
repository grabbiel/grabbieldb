#!/bin/bash

INIT_FLAG="/var/lib/grabbiel-db/.initialized"
if [ -f "$INIT_FLAG" ]; then
  echo "Database already intialized. Use migrate_db.sh for updates."
  exit 0
fi

DB_PATH="/var/lib/grabbiel-db/content.db"
SCHEMA_PATH="/etc/grabbiel/db-scripts/init_db.sql"
BACKUP_DIR="/var/backups/grabbiel-db"

# Create backup (if possible)
if [-f "$DB_PATH"]; then
  echo "Creating backup of existing database ... "
  timestamp=$(date +%Y%m%d_%H%M%S)
  cp "$DB_PATH" "$BACKUP_DIR/content_backup_$timestamp.db"
fi

# (re)Initialize db
echo "Initializing db ..."
sqlite3 "$DB_PATH" <"$SCHEMA_PATH"

# set proper permission
chmod 640 "$DB_PATH"

echo "Database intialization complete."

touch "$INIT_FLAG"
