#!/bin/bash

# Compile the admin interface
g++ -std=c++17 -o db_admin db_admin.cpp -lsqlite3

# Create a systemd service for auto-start
cat >/tmp/db-admin.service <<'EOF'
[Unit]
Description=SQLite Database Admin Interface
After=network.target

[Service]
ExecStart=/usr/local/bin/db_admin
WorkingDirectory=/usr/local/bin
Restart=always
RestartSec=5s
User=root
Group=root

[Install]
WantedBy=multi-user.target
EOF

# Install the binary and service
sudo mv db_admin /usr/local/bin/
sudo mv /tmp/db-admin.service /etc/systemd/system/

# Set proper permissions
sudo chmod +x /usr/local/bin/db_admin

# Reload systemd and start the service
sudo systemctl daemon-reload
sudo systemctl enable db-admin
sudo systemctl start db-admin

echo "Database admin interface installed successfully"
echo "Access via SSH port forwarding: ssh -L 8888:localhost:8888 your-vm-user@your-vm-ip"
