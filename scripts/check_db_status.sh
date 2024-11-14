#!/bin/bash

DB_PATH="/var/lib/grabbiel-db/content.db"

echo "Database Status Check"
echo "===================="

# Check database size
size=$(ls -lh "$DB_PATH" | awk '{print $5}')
echo "Database size: $size"

# Check schema version
version=$(sqlite3 "$DB_PATH" "SELECT MAX(version) FROM schema_versions;")
echo "Schema version: $version"

# Check table counts
echo -e "\nTable Statistics:"
sqlite3 "$DB_PATH" "
SELECT 
    'Articles' as table_name, 
    COUNT(*) as total,
    SUM(CASE WHEN published = 1 THEN 1 ELSE 0 END) as published
FROM articles;
"

# Check disk space
echo -e "\nDisk Space:"
df -h /var/lib/grabbiel-db

# Check recent backups
echo -e "\nRecent Backups:"
ls -lh /var/backups/grabbiel-db | tail -n 5
