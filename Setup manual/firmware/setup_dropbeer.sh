#!/bin/bash


# Deamon for power on/off control
mkdir ~/Scripts
cp ./FileVault/rpi_control.py /home/pi/Scripts/rpi_control.py
sudo cp ./FileVault/rpi_control.serviceX /lib/systemd/system/rpi_control.service
sudo chmod 644 /lib/systemd/system/rpi_control.service
sudo systemctl daemon-reload
sudo systemctl enable rpi_control.service

# OpenPlotter config
#cp ./FileVault/openplotter.conf ~/.openplotter/openplotter.conf

# Argon fan hat
curl https://download.argon40.com/argon1.sh | bash

# ETEPON 7 inch display
sudo sh -c "echo '# ETEPON 7 inch display' >> /boot/config.txt"
sudo sh -c "echo 'max_usb_current=1' >> /boot/config.txt"
sudo sh -c "echo 'hdmi_force_hotplug=1' >> /boot/config.txt"
sudo sh -c "echo 'config_hdmi_boost=10' >> /boot/config.txt"
sudo sh -c "echo 'hdmi_group=2' >> /boot/config.txt"
sudo sh -c "echo 'hdmi_mode=87' >> /boot/config.txt"
sudo sh -c "echo 'hdmi_cvt 1024 600  60 6 0 0 0' >> /boot/config.txt"

# Force reboot if no cloud connection, check daily
cp ./FileVault/checkwifi.sh /home/pi/Scripts/checkwifi.sh
echo "Manually update ..."
echo "crontab -e"
echo "0 3 * * * /usr/bin/sudo -H /home/pi/Scripts/checkwifi.sh >> /dev/null 2>&1"

# Install arduino environment
sudo apt-get install arduino
# cp <arduino source code from USB> <local folder>


#sudo reboot
