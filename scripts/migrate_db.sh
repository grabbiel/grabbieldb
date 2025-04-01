#!/bin/bash

DB_PATH="/var/lib/grabbiel-db/content.db"
MIGRATIONS_DIR="/etc/grabbiel/db-scripts/migrations"

# Ensure schema_versions table exists
sqlite3 "$DB_PATH" "CREATE TABLE IF NOT EXISTS schema_versions (
  version INTEGER PRIMARY KEY,
  description TEXT,
  applied_at DATETIME DEFAULT CURRENT_TIMESTAMP
);"

# Add site_id to content_blocks if not present
HAS_SITE_ID=$(sqlite3 "$DB_PATH" "PRAGMA table_info(content_blocks);" | grep -c site_id)
if [ "$HAS_SITE_ID" -eq 0 ]; then
  echo "Adding missing column: content_blocks.site_id"
  sqlite3 "$DB_PATH" "ALTER TABLE content_blocks ADD COLUMN site_id INTEGER REFERENCES sites(id);"
fi

# Get current schema version
current_version=$(sqlite3 "$DB_PATH" "SELECT IFNULL(MAX(version), 0) FROM schema_versions;")

# Apply new migrations
for migration in "$MIGRATIONS_DIR"/*.sql; do
  [ -e "$migration" ] || continue # Skip if no SQL files exist

  filename=$(basename "$migration")
  version="${filename%%_*}" # Extract numeric prefix

  if [[ "$version" =~ ^[0-9]+$ ]] && [ "$version" -gt "$current_version" ]; then
    echo "Applying migration $migration..."

    # Backup before applying
    timestamp=$(date +%Y%m%d_%H%M%S)
    sqlite3 "$DB_PATH" ".backup /var/backups/grabbiel-db/pre_migration_${version}_${timestamp}.db"

    # Apply migration SQL
    sqlite3 "$DB_PATH" ".read $migration"

    # Record version
    description=$(basename "$migration" | cut -d'_' -f2- | sed 's/.sql$//')
    sqlite3 "$DB_PATH" "INSERT INTO schema_versions (version, description) VALUES ($version, '$description');"

    echo "Migration $version completed"
  fi
done

# Optional dry run
if [ "$1" = "--dry-run" ]; then
  echo "Dry run - would apply these migrations:"
  for migration in "$MIGRATIONS_DIR"/*.sql; do
    version=$(basename "$migration" | cut -d'_' -f1)
    if [ "$version" -gt "$current_version" ]; then
      echo "- $migration"
    fi
  done
  exit 0
fi
