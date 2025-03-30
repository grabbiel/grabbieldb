#!/bin/bash

PID_FILE=~/Library/Application\ Support/media-manager/tunnel.pid
LOG_FILE=~/Library/Logs/media-manager-connect.log

echo "Starting media manager tunnel termination process..."

# Create directory for logs if it doesn't exist
mkdir -p ~/Library/Logs
mkdir -p ~/Library/Application\ Support/media-manager

# First method: Try using the PID file
if [ -f "$PID_FILE" ]; then
  PID=$(cat "$PID_FILE")
  echo "Found PID file with process ID: $PID"
  echo "$(date): Attempting to close tunnel with PID: $PID" >>"$LOG_FILE"

  # Check if the process exists
  if ps -p $PID >/dev/null; then
    echo "Process exists, attempting to terminate..."
    if kill $PID 2>/dev/null; then
      echo "Successfully terminated SSH tunnel via PID"
      echo "$(date): Successfully terminated SSH tunnel via PID" >>"$LOG_FILE"
    else
      echo "Failed to terminate process with kill command"
      echo "$(date): Failed to terminate process with kill command" >>"$LOG_FILE"
    fi
  else
    echo "Process $PID does not exist"
    echo "$(date): Process $PID from PID file does not exist" >>"$LOG_FILE"
  fi

  # Remove PID file
  rm -f "$PID_FILE"
  echo "Removed PID file"
else
  echo "No PID file found, trying alternative method"
  echo "$(date): No PID file found, using alternative method" >>"$LOG_FILE"
fi

# Second method: Find and kill all matching SSH processes
echo "Searching for SSH tunnel processes on port 8889..."
PIDS=$(ps aux | grep "ssh.*8889:localhost:8889" | grep -v grep | awk '{print $2}')

if [ -n "$PIDS" ]; then
  echo "Found SSH tunnel processes: $PIDS"
  echo "$(date): Found SSH tunnel processes: $PIDS" >>"$LOG_FILE"

  for pid in $PIDS; do
    echo "Killing process $pid..."
    if kill -9 $pid 2>/dev/null; then
      echo "Successfully terminated process $pid"
      echo "$(date): Successfully terminated process $pid" >>"$LOG_FILE"
    else
      echo "Failed to terminate process $pid"
      echo "$(date): Failed to terminate process $pid" >>"$LOG_FILE"
    fi
  done
else
  echo "No matching SSH tunnel processes found"
  echo "$(date): No matching SSH tunnel processes found" >>"$LOG_FILE"
fi

# Verify port 8889 is no longer in use
if netstat -an | grep "LISTEN" | grep -q "8889"; then
  echo "WARNING: Port 8889 is still in use!"
  echo "$(date): WARNING: Port 8889 is still in use!" >>"$LOG_FILE"
else
  echo "Port 8889 is now free"
  echo "$(date): Port 8889 is now free" >>"$LOG_FILE"
fi

echo "Media manager tunnel termination process complete"
