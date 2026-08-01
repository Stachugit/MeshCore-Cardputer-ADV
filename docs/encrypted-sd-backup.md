# Encrypted SD backup and restore

The Cardputer ADV companion firmware can save an authenticated, encrypted
snapshot of its complete SPIFFS partition to a microSD card. The snapshot
includes node and radio settings, channels and channel secrets, contacts,
messages, the node identity/private key, and other SPIFFS-managed state. UI
brightness and theme preferences are included separately inside the same
encrypted archive.

## Create a backup

1. Insert a FAT-formatted microSD card.
2. Open **Settings > SD Backup > Encrypted backup**.
3. Enter a passphrase of at least eight characters.
4. Enter the same passphrase again to confirm it.

The firmware writes the archive to `/meshcore/full-backup.mcb`. An existing
archive is replaced only after the new temporary file has been written
successfully.

Keep both the archive and passphrase secure. The passphrase cannot be recovered,
and the archive contains the node identity and channel secrets needed to assume
the backed-up node's identity.

## Restore a backup

1. Insert the microSD card containing `/meshcore/full-backup.mcb`.
2. Open **Settings > SD Backup > Full restore**.
3. Enter the backup passphrase.

The firmware authenticates the complete archive and checks that its recorded
SPIFFS partition size matches the device before changing flash. A wrong
passphrase, modified archive, or incompatible partition layout is rejected.
After a successful restore, the device restarts automatically.

Restore replaces the complete SPIFFS snapshot. Use an archive created with the
same partition layout and keep a separate copy of important backups.

## Archive protection

- AES-256-GCM provides encryption and tamper detection.
- PBKDF2-HMAC-SHA-256 derives the key from the passphrase using 100,000
  iterations and a random 16-byte salt.
- Each archive uses a random 12-byte nonce and a 16-byte authentication tag.
- The passphrase and derived-key buffers are cleared after use.

The identity/private key is encrypted, not hashed. A one-way hash could verify
data but could not restore the identity.

## M5Launcher installation

When installing a firmware update through M5Launcher, do not install or replace
SPIFFS if you want to preserve the live settings already on the device. The
backup archive itself remains on the microSD card.
