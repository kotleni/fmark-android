#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Environment
export JAVA_HOME="$HOME/jdks/jdk-21.0.12.1+1"
export ANDROID_HOME="$HOME/Android/sdk"
export PATH="$JAVA_HOME/bin:$ANDROID_HOME/platform-tools:$ANDROID_HOME/cmdline-tools/latest/bin:$PATH"

KEYSTORE_DIR="$HOME/files/keystores"
KEYSTORE_FILE="$KEYSTORE_DIR/fmark-release.keystore"
KEYSTORE_PROPS="$PROJECT_DIR/keystore.properties"
KEY_ALIAS="fmark"

# Generate a release keystore on first run
if [ ! -f "$KEYSTORE_FILE" ]; then
    echo "No keystore found. Generating release keystore..."
    mkdir -p "$KEYSTORE_DIR"
    KEYSTORE_PASSWORD="$(head -c 32 /dev/urandom | base64 | tr -dc 'A-Za-z0-9' | head -c 24)"
    "$JAVA_HOME/bin/keytool" -genkeypair -v \
        -keystore "$KEYSTORE_FILE" \
        -alias "$KEY_ALIAS" \
        -keyalg RSA -keysize 2048 -validity 10000 \
        -storepass "$KEYSTORE_PASSWORD" -keypass "$KEYSTORE_PASSWORD" \
        -dname "CN=fmark, OU=Dev, O=fmark, L=N/A, ST=N/A, C=US"
    cat > "$KEYSTORE_PROPS" <<-EOF
storeFile=$KEYSTORE_FILE
storePassword=$KEYSTORE_PASSWORD
keyAlias=$KEY_ALIAS
keyPassword=$KEYSTORE_PASSWORD
EOF
    chmod 600 "$KEYSTORE_FILE" "$KEYSTORE_PROPS"
    echo "Keystore created at $KEYSTORE_FILE"
fi

# Build the signed release APK
cd "$PROJECT_DIR"
./gradlew assembleRelease

# Copy the signed APK
DEST="./fmark.apk"
mkdir -p "$HOME/files/apks"
cp "$PROJECT_DIR/app/build/outputs/apk/release/app-release.apk" "$DEST"
echo "Signed APK copied to $DEST"
