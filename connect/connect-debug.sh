#!/bin/bash
# improved-connect.sh

# Set default values and allow overrides with environment variables
SSH_KEY=$HOME/.ssh/github_actions_deploy
VM_USER=${VM_USER}
VM_IP=${VM_IP}
LOCAL_PORT=8888
REMOTE_PORT=8888

echo "Connecting to $VM_USER@$VM_IP..."
echo "Using SSH key: $SSH_KEY"

# Test basic SSH connection first
echo "Testing basic SSH connection..."
if ssh -i "$SSH_KEY" "$VM_USER@$VM_IP" "echo 'Connection successful'" 2>/dev/null; then
  echo "Basic SSH connection works!"
else
  echo "ERROR: Basic SSH connection failed. Check your credentials and network connectivity."
  exit 1
fi

# Now try setting up the SSH tunnel
echo "Setting up port forwarding..."
ssh -i "$SSH_KEY" -L $LOCAL_PORT:localhost:$REMOTE_PORT "$VM_USER@$VM_IP" -N &
TUNNEL_PID=$!

# Wait for tunnel to establish
echo "Waiting for tunnel to establish (5 seconds)..."
sleep 5

# Check if our tunnel process is still running
if ps -p $TUNNEL_PID >/dev/null; then
  echo "SSH tunnel process is running (PID: $TUNNEL_PID)"
else
  echo "ERROR: SSH tunnel process died. Check for errors above."
  exit 1
fi

# Check if port is now listening
echo "Checking if port $LOCAL_PORT is now listening..."
if netstat -an | grep "LISTEN" | grep "$LOCAL_PORT" >/dev/null; then
  echo "Success! Port $LOCAL_PORT is now listening."
else
  echo "WARNING: Port $LOCAL_PORT doesn't appear to be listening. The tunnel might not be working correctly."
fi

# Try accessing the admin interface
echo "Attempting to access the admin interface..."
if curl -s --head --fail http://localhost:$LOCAL_PORT >/dev/null; then
  echo "Success! The admin interface is responding."

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
else
  echo "ERROR: Cannot connect to the admin interface. The service might not be running on the VM."
fi

echo "Press Ctrl+C to close the SSH tunnel when finished"

# Set up a trap to kill the SSH tunnel when the script exits
trap "echo 'Closing SSH tunnel...'; kill $TUNNEL_PID 2>/dev/null" EXIT INT TERM

# Keep the script running to maintain the tunnel
while true; do
  sleep 1
done
