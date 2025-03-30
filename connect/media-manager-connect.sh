#!/bin/bash
# media-manager-connect.sh

SSH_KEY=~/.ssh/github_actions_deploy
VM_USER=${VM_USER:-fcruzado22}
VM_IP=${VM_IP:-35.197.15.118}
LOCAL_PORT=8889
REMOTE_PORT=8889

# Create directories for logs and PID storage
mkdir -p ~/Library/Logs
mkdir -p ~/Library/Application\ Support/media-manager

LOG_FILE=~/Library/Logs/media-manager-connect.log
PID_FILE=~/Library/Application\ Support/media-manager/tunnel.pid

echo "$(date): Starting connection to Media Manager Interface..." >"$LOG_FILE"
echo "Setting up connection to Media Manager Interface..."

# Start SSH tunnel in the background with no time limit
ssh -f -o ExitOnForwardFailure=yes -o ServerAliveInterval=60 -i "$SSH_KEY" -L $LOCAL_PORT:localhost:$REMOTE_PORT "$VM_USER@$VM_IP" "sleep infinity" &
SSH_PID=$!

# Save PID to file for later termination
echo $SSH_PID >"$PID_FILE"
echo "$(date): Created SSH tunnel with PID: $SSH_PID" >>"$LOG_FILE"

# Wait a moment for the tunnel to establish
sleep 2

# Check if port is listening
if netstat -an | grep "LISTEN" | grep -q "$LOCAL_PORT"; then
  echo "$(date): Port forwarding established successfully" >>"$LOG_FILE"
  echo "Port forwarding established successfully!"

  # Open browser
  echo "Opening browser..."
  case "$(uname -s)" in
  Darwin)
    open "http://localhost:$LOCAL_PORT"
    ;;
  Linux)
    xdg-open "http://localhost:$LOCAL_PORT" 2>/dev/null ||
      firefox "http://localhost:$LOCAL_PORT" 2>/dev/null ||
      google-chrome "http://localhost:$LOCAL_PORT" 2>/dev/null
    ;;
  CYGWIN* | MINGW* | MSYS*)
    start "http://localhost:$LOCAL_PORT"
    ;;
  *)
    echo "Please open http://localhost:$LOCAL_PORT in your browser"
    ;;
  esac

  echo "$(date): Browser opened" >>"$LOG_FILE"
else
  echo "$(date): Port forwarding failed" >>"$LOG_FILE"
  echo "ERROR: Failed to establish port forwarding to Media Manager Interface"
  # Clean up on failure
  kill $SSH_PID 2>/dev/null
  rm -f "$PID_FILE"
  exit 1
fi

echo "Media Manager Interface connection established."
echo "The connection will remain active until you close this terminal window or run media-manager-close.sh"
echo "Press Ctrl+C to close the connection when done"

# Set up trap to clean up on exit
trap 'echo "Closing connection..."; kill $SSH_PID 2>/dev/null; rm -f "$PID_FILE"; echo "$(date): Connection closed by user" >> "$LOG_FILE"; exit 0' INT TERM EXIT

# Keep the script running to maintain the tunnel
while true; do
  sleep 1
done
