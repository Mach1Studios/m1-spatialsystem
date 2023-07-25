#! /bin/bash

# MACH1 SPATIAL SYSTEM INSTALLER BUILD PIPELINE & CODESIGNING ENV
# Run this command to package the installer for Mach1 Spatial System
# for release

# TODO 
# - Add error checking/handling

set -e

source ~/m1-globallocal.sh

# Installer 
echo "Package Installer?"
select yn in "Yes" "No"; do
    case $yn in
        Yes ) 
            echo "### PACKAGING INSTALLER ###";
            cd $M1WORKFLOW_PATH/installer/osx;
            packagesbuild -v Mach1\ Spatial\ System\ Installer.pkgproj;
            cd build/;
            mkdir -p signed;
            productsign --sign "Developer ID Installer: $APPLE_ID" "Mach1 Spatial System Installer.pkg" "signed/Mach1 Spatial System Installer.pkg";
            echo "### FINISHED FULL AAX/VST/VST3/AU INSTALLER ###";
            break;;
        No ) break;;
    esac
done 

# NOTARIZING (macOS >10.14)
echo "Do you want to submit installer for Notarization?"
echo "Please only do this with access to Xcode 10+"
select yn in "Yes" "No"; do
    case $yn in
        Yes ) 
            echo "### NOTARIZING INSTALLER ###";
            cd $M1WORKFLOW_PATH/installer/osx/build/;
            pkgutil --check-signature "signed/Mach1 Spatial System Installer.pkg";

            echo "### OPENING SD NOTARY FOR MANUAL SIGNING ###";
            open /Applications/SD\ Notary.app;
            # Notarize the app with macOS GateKeeper (Requires newer macOS)
            #xcrun altool --notarize-app --primary-bundle-id "com.mach1.spatialsystem" --username $APPLE_USERNAME --password $ALTOOL_APPPASS -itc_provider $APPLE_TEAM_CODE --file "signed/Mach1 Spatial System Installer.pkg";
            #echo "### NOTARIZATION SUBMITTED ###";
            #echo "COMMAND TO CHECK NOTARIZATION: ";
            #echo "xcrun altool --notarization-info HASH --username $APPLE_USERNAME --password $ALTOOL_APPPASS";
            #echo "Check the log URL returned by the above command for specifics";
            break;;
        No ) break;;
    esac
done 

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