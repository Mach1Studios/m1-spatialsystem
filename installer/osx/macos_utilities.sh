#!/bin/bash

# MACH1 SPATIAL SYSTEM 
# Common build and distribution utilities

POSITIONAL_ARGS=()

while [[ $# -gt 0 ]]; do
  case $1 in
    -f|--filename)
      NAME="$2"
      shift # past argument
      shift # past value
      ;;
    -e|--extension)
      EXT="$2"
      shift # past argument
      shift # past value
      ;;
    -p|--path)
      PATH="$2"
      shift # past argument
      shift # past value
      ;;
    -k|--keychain-profile)
      KEYCHAINPROFILE="$2"
      shift # past argument
      shift # past value
      ;;
    --apple-id)
      APPLE_ID="$2"
      shift # past argument
      shift # past value
      ;;
    --apple-app-pass)
      APPLE_APP_PASS="$2"
      shift # past argument
      shift # past value
      ;;
    -t|--apple_team)
      APPLE_TEAM="$2"
      shift # past argument
      shift # past value
      ;;
    -z|--zip)
      ZIP="$2"
      shift # past argument
      shift # past value
      ;;
    -*|--*)
      echo "Unknown option $1"
      exit 1
      ;;
    *)
      POSITIONAL_ARGS+=("$1") # save positional arg
      shift # past argument
      ;;
  esac
done

set -- "${POSITIONAL_ARGS[@]}" # restore positional parameters

if [[ -n $1 ]]; then
    echo "Last line of file specified as non-opt/last argument:"
    tail -1 "$1"
fi

echo "Notarizing: ${PATH}/${NAME}${EXT}..."

/usr/bin/xcrun notarytool submit --wait --timeout 10m --keychain-profile ${KEYCHAINPROFILE} --apple-id ${APPLE_ID} --password ${APPLE_APP_PASS} --team-id ${APPLE_TEAM} ${PATH}/${NAME}${EXT}${ZIP}

# if ! /usr/bin/grep -q '"status":"Accepted"' notarize.json; then
#     echo "Notarization failed"
#     /bin/cat notarize.json
#     exit 1
# fi

# delete tmp file
#rm notarize.json

echo "Notarization success, now stapling the installer ..."

/usr/bin/xcrun stapler staple ${PATH}/${NAME}${EXT}