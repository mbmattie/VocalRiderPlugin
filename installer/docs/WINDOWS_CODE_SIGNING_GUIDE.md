# magic.RIDE: Windows Code Signing Guide

This guide walks you through signing your Windows installer and plugin to avoid "Windows protected your PC" / "suspicious developer" SmartScreen warnings.

---

## Why Code Sign?

Without signing, users see:
- **SmartScreen warning:** "Windows protected your PC" / "More info" required to run
- **Reputation:** "Unknown publisher" or "Not commonly downloaded"
- **Trust:** Signed software is more trusted and installs with fewer prompts

---

## Types of Code Signing Certificates

| Type | SmartScreen | Cost | Verification |
|------|-------------|------|---------------|
| **EV (Extended Validation)** | Best reputation potential | ~$300–500/year | Business vetting, hardware token |
| **OV (Organization Validation)** | Builds reputation over time | ~$100–300/year | Business vetting |

**Important (2024+):** Microsoft no longer gives EV certificates automatic SmartScreen bypass. Both EV and OV builds need to build **reputation** (downloads, installs) over time. EV still establishes stronger identity and may build trust faster, but new certs can trigger warnings initially.

---

## Step 1: Obtain a Code Signing Certificate

### Option A: EV Certificate (Recommended for commercial software)

1. **Choose a Certificate Authority (CA):**
   - [DigiCert](https://www.digicert.com/signing/code-signing-certificates)
   - [Sectigo (Comodo)](https://sectigo.com/ssl-certificates-tls/code-signing)
   - [GlobalSign](https://www.globalsign.com/code-signing-certificate)
   - [SSL.com](https://www.ssl.com/certificates/code-signing/)

2. **Requirements:**
   - Legal business entity (LLC, Inc., etc.)
   - D-U-N-S number (from Dun & Bradstreet)
   - Business verification (phone, docs)
   - **Hardware token** (USB key) – sent by CA after approval

3. **Timeline:** Typically 1–5 business days after submission.

### Option B: OV Certificate

- Similar process, often faster
- Lower cost
- May take longer to build SmartScreen reputation

---

## Step 2: Sign Your Installer & Plugin

### What to Sign

1. **Installer executable** (e.g. `magic.RIDE-1.0.0-Windows-Setup.exe` from Inno Setup)
2. **VST3 plugin** (the `.dll` inside `magic.RIDE.vst3`)
3. **AAX plugin** (if you distribute AAX on Windows) – the `.aaxplugin` bundle contents
4. **Standalone app** (if you ship one)

### Using SignTool (included with Visual Studio)

1. **Open Developer Command Prompt** (or regular terminal with VS tools in PATH).

2. **Basic signing command:**
   ```cmd
   signtool sign /tr http://timestamp.digicert.com /td sha256 /fd sha256 /f "C:\path\to\your_certificate.pfx" /p "your_password" "C:\path\to\Installer.exe"
   ```

3. **Timestamp servers** (use one):
   - DigiCert: `http://timestamp.digicert.com`
   - Sectigo: `http://timestamp.sectigo.com`
   - GlobalSign: `http://timestamp.globalsign.com`

4. **For EV cert on hardware token:**
   ```cmd
   signtool sign /tr http://timestamp.digicert.com /td sha256 /fd sha256 /sha1 "thumbprint_of_your_cert" "C:\path\to\Installer.exe"
   ```
   (Use `certmgr` or `certutil -store My` to find the thumbprint.)

### Inno Setup: Auto-Sign the Installer

Add to your `.iss` file (replace paths and values):

```iss
[Setup]
; ... existing options ...
SignTool=signtool sign /tr http://timestamp.digicert.com /td sha256 /fd sha256 /f "C:\path\to\cert.pfx" /p "password" $f
```

Or use a batch/PowerShell wrapper that calls `iscc` and then `signtool` on the output.

---

## Step 3: Build SmartScreen Reputation

Even with a valid cert, new or rarely-downloaded software can still trigger SmartScreen. To improve reputation:

1. **Ensure proper signing:**
   - Use a valid timestamp
   - Sign with SHA-256
   - Include full certificate chain

2. **Submit to Microsoft (optional but helpful):**
   - [Submit a file for analysis](https://www.microsoft.com/en-us/wdsi/filesubmission)
   - Use "Driver" or "Software" as appropriate
   - Explain that the file is legitimately signed and safe

3. **Build download/install volume:** More installs and positive feedback help reputation over time.

4. **Publish via known channels:** Distribution through your website, stores, or platforms that users trust can also help.

---

## Step 4: CI/CD Integration (Optional)

To sign automatically in your build pipeline:

1. **Store the certificate securely** (e.g. Azure Key Vault, GitHub Actions secret, or secure vault).
2. **Use `signtool`** in your build script after building the installer.
3. **Timestamp** every signed binary.

Example GitHub Actions step:
```yaml
- name: Sign installer
  run: |
    signtool sign /tr http://timestamp.digicert.com /td sha256 /fd sha256 `
      /f ${{ secrets.CERT_PFX_PATH }} /p ${{ secrets.CERT_PASSWORD }} `
      installer/output/magic.RIDE-1.0.0-Windows-Setup.exe
```

---

## Checklist

- [ ] Obtain EV or OV code signing certificate
- [ ] Install certificate (or use hardware token)
- [ ] Sign the Inno Setup installer
- [ ] Sign the VST3 `.dll` (and AAX if applicable)
- [ ] Use a timestamp server on all signed files
- [ ] Test on a clean Windows VM
- [ ] Submit installer to Microsoft SmartScreen if needed
- [ ] Document signing steps for future releases

---

## Troubleshooting

**"The certificate chain was issued by an authority that is not trusted"**  
Install the CA’s root and intermediate certificates on the build machine, or ensure the full chain is in your PFX.

**"Access denied" / "The specified credentials are invalid"**  
For hardware tokens, confirm the token is plugged in and the correct PIN is used. For PFX, verify the password.

**SmartScreen still warns after signing**  
Normal for new certificates. Ensure you timestamp, use a reputable CA, and allow time for reputation to build. You can submit the file to Microsoft for review.

---

## Useful Links

- [Microsoft SignTool docs](https://learn.microsoft.com/en-us/windows/win32/seccrypto/signtool)
- [File submission for SmartScreen](https://www.microsoft.com/en-us/wdsi/filesubmission)
- [Code signing best practices](https://learn.microsoft.com/en-us/windows/win32/seccrypto/cryptography-tools)
