#!/bin/bash

DB_PATH="/var/lib/grabbiel-db/content.db"
MIGRATIONS_DIR="/etc/grabbiel/db-scripts/migrations"

# Get current schema version (hopeful?)
current_version=$(sqlite3 "$DB_PATH" "SELECT MAX(version) FROM schema_versions;")

# Find and apply new migrations
for migration in "$MIGRATIONS_DIR"/*.sql; do
  version=$(basename "$migration" | cut -d'_' -f1)
  if [ "$version" -gt "$current_version" ]; then
    echo "Applying migration $migration..."

    # Create backup before migration
    timestamp=$(date +%Y%m%d_%H%M%S)
    sqlite3 "$DB_PATH" ".backup '/var/backups/grabbiel-db/pre_migration_${version}_${timestamp}.db'"

    # Apply migration
    sqlite3 "$DB_PATH" ".read $migration"

    # Update schema version
    description=$(basename "$migration" | cut -d'_' -f2- | sed 's/.sql$//')
    sqlite3 "$DB_PATH" "INSERT INTO schema_versions (version, description) VALUES ($version, '$description');"

    echo "Migration $version completed"
  fi
done

if [ "$1" = "--dry-run" ]; then
  echo "Dry run - would apply these migrations:"
  # List pending migrations
  for migration in "$MIGRATIONS_DIR"/*.sql; do
    version=$(basename "$migration" | cut -d'_' -f1)
    if [ "$version" -gt "$current_version" ]; then
      echo "- $migration"
    fi
  done
  exit 0
fi
