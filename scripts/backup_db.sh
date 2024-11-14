#!/bin/bash

DB_PATH="/var/lib/grabbiel-db/content.db"
BACKUP_DIR="/var/backups/grabbiel-db"
MAX_BACKUPS=7

# Create backup
timestamp=$(date +%Y%m%d_%H%M%S)
backup_file="$BACKUP_DIR/content_backup_$timestamp.db"

# use sqlite3 to create consistent backup
sqlite3 "$DB_PATH" ".backup '$backup_file'"

# compress
gzip "$backup_file"

# remove old backups
ls -t "$BACKUP_DIR"/content_backup_*.db.gz | tail -n +$((MAX_BACKUPS + 1)) | xargs -r rm

echo "Backup completed: ${backup_file}.gz"
