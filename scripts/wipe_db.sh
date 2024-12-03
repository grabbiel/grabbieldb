#!/bin/bash

# Exit on any error
set -e

# Configuration variables
DB_PATH="/var/lib/grabbiel-db/content.db"
BACKUP_DIR="/var/backups/grabbiel-db"
SCRIPTS_DIR="/etc/grabbiel/db-scripts"

# Function to create backup before wiping
create_backup() {
  local timestamp=$(date +%Y%m%d_%H%M%S)
  local backup_file="${BACKUP_DIR}/pre_wipe_${timestamp}.db"

  echo "Creating backup before database wipe..."
  if [ -f "$DB_PATH" ]; then
    if sqlite3 "$DB_PATH" ".backup '$backup_file'"; then
      echo "Backup created successfully at: $backup_file"
    else
      echo "Error creating backup. Aborting wipe process."
      exit 1
    fi
  else
    echo "No existing database found at $DB_PATH"
  fi
}

# Function to wipe database and related files
wipe_database() {
  echo "Starting database cleanup process..."

  # Stop any running processes that might be using the database
  echo "Checking for and stopping any processes using the database..."
  lsof "$DB_PATH" 2>/dev/null | awk 'NR>1 {print $2}' | xargs -r kill -9

  # Remove main database file
  if [ -f "$DB_PATH" ]; then
    echo "Removing main database file..."
    rm -f "$DB_PATH"
  fi

  # Remove any journal or WAL files
  echo "Cleaning up additional database files..."
  rm -f "${DB_PATH}-wal"
  rm -f "${DB_PATH}-shm"
  rm -f "${DB_PATH}-journal"

  # Clean up any temporary files
  echo "Removing temporary files..."
  find "$(dirname "$DB_PATH")" -name "*.tmp" -delete

  echo "Database cleanup completed successfully."
}

# Main execution
main() {
  echo "=== Starting Database Wipe Process ==="

  # Check if script is run as root or with sudo
  if [ "$EUID" -ne 0 ]; then
    echo "Please run this script as root or with sudo"
    exit 1
  fi

  # Check if directories exist
  if [ ! -d "$BACKUP_DIR" ]; then
    echo "Creating backup directory..."
    mkdir -p "$BACKUP_DIR"
  fi

  # Verify write permissions
  if [ ! -w "$(dirname "$DB_PATH")" ]; then
    echo "Error: No write permission in database directory"
    exit 1
  fi

  # Execute backup and wipe
  create_backup
  wipe_database

  echo "=== Database Wipe Process Completed ==="
  echo "Note: To recreate the database, run the setup script at:"
  echo "$SCRIPTS_DIR/setup_db.sh"
}

# Run main function
main "$@"
