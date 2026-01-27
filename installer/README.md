# magic.RIDE Installer Build System

This directory contains scripts to create a professional macOS installer package for magic.RIDE.

## Quick Start (Without Signing)

```bash
# Build the plugin first (Release mode recommended)
cd /path/to/VocalRiderPlugin
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# Build the installer
cd installer
./build_installer.sh
```

The unsigned installer will be created at: `output/magic.RIDE-1.0.0-Installer.pkg`

> **Note:** Unsigned installers will trigger macOS Gatekeeper warnings. Users will need to right-click → Open or allow it in System Settings → Privacy & Security.

---

## Full Signing & Notarization (Recommended)

### Prerequisites

1. **Apple Developer Account** ($99/year) - [developer.apple.com](https://developer.apple.com)

2. **Install Developer ID Certificates:**
   - Log into Xcode → Settings → Accounts → Manage Certificates
   - Create "Developer ID Application" certificate
   - Create "Developer ID Installer" certificate

3. **Create App-Specific Password:**
   - Go to [appleid.apple.com](https://appleid.apple.com)
   - Security → App-Specific Passwords → Generate
   - Save this password securely

4. **Find Your Team ID:**
   - Go to [developer.apple.com/account](https://developer.apple.com/account)
   - Membership → Team ID (10 characters)

### Configure the Script

Edit `sign_and_notarize.sh` and set these values:

```bash
DEVELOPER_ID_APP="Developer ID Application: Your Name (XXXXXXXXXX)"
DEVELOPER_ID_INSTALLER="Developer ID Installer: Your Name (XXXXXXXXXX)"
APPLE_ID="your@email.com"
TEAM_ID="XXXXXXXXXX"
APP_PASSWORD="xxxx-xxxx-xxxx-xxxx"
```

Or set them as environment variables before running.

### Run

```bash
./sign_and_notarize.sh
```

This will:
1. Sign the AU and VST3 plugins
2. Build and sign the installer package
3. Submit to Apple for notarization
4. Staple the notarization ticket

The final installer will work without any Gatekeeper warnings!

---

## What Gets Installed

The installer places plugins in the user's Library (no admin password required):

| Plugin | Location |
|--------|----------|
| AU | `~/Library/Audio/Plug-Ins/Components/magic.RIDE.component` |
| VST3 | `~/Library/Audio/Plug-Ins/VST3/magic.RIDE.vst3` |

---

## Files in This Directory

| File | Purpose |
|------|---------|
| `build_installer.sh` | Creates the PKG installer |
| `sign_and_notarize.sh` | Signs and notarizes for distribution |
| `output/` | Generated installers appear here |
| `staging/` | Temporary build directory (auto-cleaned) |

---

## Troubleshooting

### "Developer ID certificate not found"
- Open Keychain Access and verify your certificates are installed
- The name must match exactly, including the Team ID in parentheses

### "Notarization failed"
- Check that hardened runtime is enabled
- Ensure no unsigned nested code
- View detailed log: `xcrun notarytool log <submission-id> --apple-id ... --team-id ...`

### "The installer is damaged"
- The package wasn't properly signed or notarized
- Re-run the full signing process

---

## Version History

- 1.0.0 - Initial release
