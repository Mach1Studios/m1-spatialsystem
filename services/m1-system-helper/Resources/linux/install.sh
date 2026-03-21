#!/bin/bash
# Linux Installation Script for M1-System-Helper
# Run with sudo

set -e

echo "Installing M1-System-Helper Linux Service..."

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Please run as root (with sudo)"
    exit 1
fi

# Copy binary to system location
echo "Installing binary to /usr/local/bin/"
cp ../../build-dev/m1-system-helper /usr/local/bin/
chmod +x /usr/local/bin/m1-system-helper

# Install systemd service files
echo "Installing systemd service files..."
cp m1-system-helper.service /etc/systemd/system/
cp m1-system-helper.socket /etc/systemd/system/

# Reload systemd daemon
echo "Reloading systemd daemon..."
systemctl daemon-reload

# Enable socket activation (service will start on-demand)
echo "Enabling socket activation..."
systemctl enable m1-system-helper.socket
systemctl start m1-system-helper.socket

# The service itself should NOT be enabled for auto-start
# It will start automatically when socket is accessed
systemctl disable m1-system-helper.service || true

echo "Installation complete!"
echo ""
echo "Service configuration:"
echo "  - Socket activation enabled: m1-system-helper.socket"
echo "  - Service will start on-demand when Mach1 apps connect"
echo "  - Service will auto-exit after 5 minutes of inactivity"
echo ""
echo "To check status:"
echo "  sudo systemctl status m1-system-helper.socket"
echo "  sudo systemctl status m1-system-helper.service"
echo ""
echo "To view logs:"
echo "  sudo journalctl -u m1-system-helper -f" 