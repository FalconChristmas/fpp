#!/bin/bash
#
# Close loop #209 by adding common to .bashrc
#

sudo bash -c "echo >> /home/fpp/.bashrc"
sudo bash -c "echo '. /opt/fpp/scripts/common' >> /home/fpp/.bashrc"
sudo bash -c "echo >> /home/fpp/.bashrc"
