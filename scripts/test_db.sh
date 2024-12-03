#!/bin/bash

DB_PATH="/var/lib/grabbiel-db/content.db"

# Check if database exists
if [ ! -f "$DB_PATH" ]; then
  echo "Error: Database file does not exist at $DB_PATH"
  exit 1
fi

# Check if we can connect to the database
if ! sqlite3 "$DB_PATH" "SELECT sqlite_version();" >/dev/null 2>&1; then
  echo "Error: Cannot connect to database at $DB_PATH"
  exit 1
fi

echo "Testing database connection ..."
sqlite3 "$DB_PATH" "SELECT sqlite_version();"

echo "Testing content type creation ..."
sqlite3 "$DB_PATH" "INSERT OR IGNORE INTO content_types (type) VALUES ('article');"

echo "Testing content block creation ..."
sqlite3 "$DB_PATH" "
INSERT INTO content_blocks(
  title,
  url_slug,
  type_id,
  status
) VALUES(
  'Test Article',
  'test-article',
  (SELECT id FROM content_types WHERE type = 'article'),
  'published'
);"

echo "Verifying content creation ..."
sqlite3 "$DB_PATH" "SELECT id, title, status FROM content_blocks WHERE title = 'Test Article';"

echo "Database test complete!"
