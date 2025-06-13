#!/bin/bash
# connect_db_admin.sh

SSH_KEY=$HOME/.ssh/github_actions_deploy
VM_USER=${VM_USER}
VM_IP=${VM_IP}

echo "Connecting to $VM_USER@$VM_IP..."

# Open SSH tunnel in background
ssh -f -N -L 8888:localhost:8888 -i "$SSH_KEY" "$VM_USER@$VM_IP"

# Wait a moment for the tunnel to establish
sleep 1

# Open default browser to the forwarded port
case "$(uname -s)" in
Darwin)
  open "http://localhost:8888"
  ;;
Linux)
  xdg-open "http://localhost:8888" 2>/dev/null ||
    firefox "http://localhost:8888" 2>/dev/null ||
    google-chrome "http://localhost:8888" 2>/dev/null
  ;;
CYGWIN* | MINGW* | MSYS*)
  start "http://localhost:8888"
  ;;
*)
  echo "Open your browser and navigate to: http://localhost:8888"
  ;;
esac

echo "Press Ctrl+C to close the SSH tunnel when finished"
# Find and kill the tunnel process on exit
trap 'kill $(ps -ef | grep "ssh -f -N -L 8888:localhost:8888" | grep -v grep | awk "{print \$2}")' EXIT

# Keep the script running to maintain the tunnel
while true; do
  sleep 1
done
