#!/bin/bash

# Ensure required directories exist
sudo mkdir -p /tmp/grabbiel-uploads
sudo mkdir -p /usr/local/bin

g++ -std=c++17 -o media_manager media_manager.cpp -lsqlite3

# Create systemd service file
cat >/tmp/media-manager.service <<'EOF'
[Unit]
Description=Media Manager Web Interface
After=network.target

[Service]
Environment="GOOGLE_APPLICATION_CREDENTIALS=/etc/google-cloud-keys/grabbiel-media-key.json"
ExecStart=/usr/local/bin/media_manager
WorkingDirectory=/usr/local/bin
Restart=always
RestartSec=5s
User=root
Group=root

[Install]
WantedBy=multi-user.target
EOF

# Install the binary and service
sudo mv media_manager /usr/local/bin/
sudo mv /tmp/media-manager.service /etc/systemd/system/

# Set proper permissions
sudo chmod +x /usr/local/bin/media_manager
sudo chmod 777 /tmp/grabbiel-uploads

# Reload systemd and start the service
sudo systemctl daemon-reload
sudo systemctl enable media-manager
sudo systemctl start media-manager

echo "Media Manager installed successfully"
echo "Access via SSH port forwarding: ssh -L 8889:localhost:8889 your-vm-user@your-vm-ip"
