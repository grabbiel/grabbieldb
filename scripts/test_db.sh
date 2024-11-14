#!/bin/bash

DB_PATH="/var/lib/grabbiel-db/content.db"

echo "Testing database connection ..."
sqlite3 "$DB_PATH" "SELECT sqlite_version();"

echo "Testing article creation ..."
sqlite3 "$DB_PATH" "INSERT INTO articles (title, content, category, published) VALUES ('Test Article', 'Test Content', 'test', 1);"

echo "Verifying article creation ..."
sqlite3 "$DB_PATH" "SELECT id, title, category FROM articles WHERE category = 'test';"

echo "Database test complete!"
