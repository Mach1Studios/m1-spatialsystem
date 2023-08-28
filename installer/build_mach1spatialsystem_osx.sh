#! /bin/bash

# MACH1 SPATIAL SYSTEM INSTALLER BUILD PIPELINE & CODESIGNING ENV
# Run this command to package the installer for Mach1 Spatial System
# for release

# TODO 
# - Add error checking/handling

set -e

source ~/m1-globallocal.sh

echo "Deploy Installer?"
select yn in "Yes" "No"; do
    case $yn in
        Yes ) 
            echo "### DEPLOYING TO S3 ###";
            cd $M1WORKFLOW_PATH/installer/osx/build;
            # version="USER INPUT"
            read -p "Version: " version
            aws s3 cp "signed/Mach1 Spatial System Installer - Notarized/Mach1 Spatial System Installer.pkg" "s3://mach1-releases/$version/Mach1 Spatial System Installer.pkg" --profile mach1 --content-disposition "Mach1 Spatial System Installer.pkg";
            echo "REMINDER: Update the Avid Store Submission per version update!"
            break;;
        No ) break;;
    esac
done