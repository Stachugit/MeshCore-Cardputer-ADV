#include <Arduino.h>
#include "DataStore.h"

#if defined(EXTRAFS) || defined(QSPIFLASH)
  #define MAX_BLOBRECS 100
#else
  #define MAX_BLOBRECS 20
#endif

DataStore::DataStore(FILESYSTEM& fs, mesh::RTCClock& clock) : _fs(&fs), _fsExtra(nullptr), _clock(&clock),
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
    identity_store(fs, "")
#elif defined(RP2040_PLATFORM)
    identity_store(fs, "/identity")
#else
    identity_store(fs, "/identity")
#endif
{
}

#if defined(EXTRAFS) || defined(QSPIFLASH)
DataStore::DataStore(FILESYSTEM& fs, FILESYSTEM& fsExtra, mesh::RTCClock& clock) : _fs(&fs), _fsExtra(&fsExtra), _clock(&clock),
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
    identity_store(fs, "")
#elif defined(RP2040_PLATFORM)
    identity_store(fs, "/identity")
#else
    identity_store(fs, "/identity")
#endif
{
}
#endif

static File openWrite(FILESYSTEM* fs, const char* filename) {
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  fs->remove(filename);
  return fs->open(filename, FILE_O_WRITE);
#elif defined(RP2040_PLATFORM)
  return fs->open(filename, "w");
#else
  return fs->open(filename, "w", true);
#endif
}

#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  static uint32_t _ContactsChannelsTotalBlocks = 0;
#endif

void DataStore::begin() {
#if defined(RP2040_PLATFORM)
  identity_store.begin();
#endif

#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  _ContactsChannelsTotalBlocks = _getContactsChannelsFS()->_getFS()->cfg->block_count;
  checkAdvBlobFile();
  #if defined(EXTRAFS) || defined(QSPIFLASH)
  migrateToSecondaryFS();
  #endif
#else
  // init 'blob store' support
  _fs->mkdir("/bl");
#endif
}

#if defined(ESP32)
  #include <SPIFFS.h>
#if defined(M5STACK_CARDPUTER)
  #include <Preferences.h>
  #include <esp_partition.h>
  #include <esp_random.h>
  #include <mbedtls/gcm.h>
  #include <mbedtls/md.h>
  #include <mbedtls/pkcs5.h>
#endif
#elif defined(RP2040_PLATFORM)
  #include <LittleFS.h>
#elif defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  #if defined(QSPIFLASH)
    #include <CustomLFS_QSPIFlash.h>
  #elif defined(EXTRAFS)
    #include <CustomLFS.h>
  #else 
    #include <InternalFileSystem.h>
  #endif
#endif

#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
int _countLfsBlock(void *p, lfs_block_t block){
      if (block > _ContactsChannelsTotalBlocks) {
        MESH_DEBUG_PRINTLN("ERROR: Block %d exceeds filesystem bounds - CORRUPTION DETECTED!", block);
        return LFS_ERR_CORRUPT;  // return error to abort lfs_traverse() gracefully
    }
  lfs_size_t *size = (lfs_size_t*) p;
  *size += 1;
    return 0;
}

lfs_ssize_t _getLfsUsedBlockCount(FILESYSTEM* fs) {
  lfs_size_t size = 0;
  int err = lfs_traverse(fs->_getFS(), _countLfsBlock, &size);
  if (err) {
    MESH_DEBUG_PRINTLN("ERROR: lfs_traverse() error: %d", err);
    return 0;
  }
  return size;
}
#endif

uint32_t DataStore::getStorageUsedKb() const {
#if defined(ESP32)
  return SPIFFS.usedBytes() / 1024;
#elif defined(RP2040_PLATFORM)
  FSInfo info;
  info.usedBytes = 0;
  _fs->info(info);
  return info.usedBytes / 1024;
#elif defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  const lfs_config* config = _getContactsChannelsFS()->_getFS()->cfg;
  int usedBlockCount = _getLfsUsedBlockCount(_getContactsChannelsFS());
  int usedBytes = config->block_size * usedBlockCount;
  return usedBytes / 1024;
#else
  return 0;
#endif
}

uint32_t DataStore::getStorageTotalKb() const {
#if defined(ESP32)
  return SPIFFS.totalBytes() / 1024;
#elif defined(RP2040_PLATFORM)
  FSInfo info;
  info.totalBytes = 0;
  _fs->info(info);
  return info.totalBytes / 1024;
#elif defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  const lfs_config* config = _getContactsChannelsFS()->_getFS()->cfg;
  int totalBytes = config->block_size * config->block_count;
  return totalBytes / 1024;
#else
  return 0;
#endif
}

File DataStore::openRead(const char* filename) {
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  return _fs->open(filename, FILE_O_READ);
#elif defined(RP2040_PLATFORM)
  return _fs->open(filename, "r");
#else
  return _fs->open(filename, "r", false);
#endif
}

File DataStore::openRead(FILESYSTEM* fs, const char* filename) {
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  return fs->open(filename, FILE_O_READ);
#elif defined(RP2040_PLATFORM)
  return fs->open(filename, "r");
#else
  return fs->open(filename, "r", false);
#endif
}

bool DataStore::removeFile(const char* filename) {
  return _fs->remove(filename);
}

bool DataStore::removeFile(FILESYSTEM* fs, const char* filename) {
  return fs->remove(filename);
}

bool DataStore::formatFileSystem() {
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  if (_fsExtra == nullptr) {
    return _fs->format();
  } else {
    return _fs->format() && _fsExtra->format();
  }
#elif defined(RP2040_PLATFORM)
  return LittleFS.format();
#elif defined(ESP32)
  return ((fs::SPIFFSFS *)_fs)->format();
#else
  #error "need to implement format()"
#endif
}

bool DataStore::loadMainIdentity(mesh::LocalIdentity &identity) {
  return identity_store.load("_main", identity);
}

bool DataStore::saveMainIdentity(const mesh::LocalIdentity &identity) {
  return identity_store.save("_main", identity);
}

void DataStore::loadPrefs(NodePrefs& prefs, double& node_lat, double& node_lon) {
  if (_fs->exists("/new_prefs")) {
    loadPrefsInt("/new_prefs", prefs, node_lat, node_lon); // new filename
  } else if (_fs->exists("/node_prefs")) {
    loadPrefsInt("/node_prefs", prefs, node_lat, node_lon);
    savePrefs(prefs, node_lat, node_lon);                // save to new filename
    _fs->remove("/node_prefs"); // remove old
  }
}

void DataStore::loadPrefsInt(const char *filename, NodePrefs& _prefs, double& node_lat, double& node_lon) {
  File file = openRead(_fs, filename);
  if (file) {
    uint8_t pad[8];

    file.read((uint8_t *)&_prefs.airtime_factor, sizeof(float));                           // 0
    file.read((uint8_t *)_prefs.node_name, sizeof(_prefs.node_name));                      // 4
    file.read(pad, 4);                                                                     // 36
    file.read((uint8_t *)&node_lat, sizeof(node_lat));                                     // 40
    file.read((uint8_t *)&node_lon, sizeof(node_lon));                                     // 48
    file.read((uint8_t *)&_prefs.freq, sizeof(_prefs.freq));                               // 56
    file.read((uint8_t *)&_prefs.sf, sizeof(_prefs.sf));                                   // 60
    file.read((uint8_t *)&_prefs.cr, sizeof(_prefs.cr));                                   // 61
    file.read(pad, 1);                                                                     // 62
    file.read((uint8_t *)&_prefs.manual_add_contacts, sizeof(_prefs.manual_add_contacts)); // 63
    file.read((uint8_t *)&_prefs.bw, sizeof(_prefs.bw));                                   // 64
    file.read((uint8_t *)&_prefs.tx_power_dbm, sizeof(_prefs.tx_power_dbm));               // 68
    file.read((uint8_t *)&_prefs.telemetry_mode_base, sizeof(_prefs.telemetry_mode_base)); // 69
    file.read((uint8_t *)&_prefs.telemetry_mode_loc, sizeof(_prefs.telemetry_mode_loc));   // 70
    file.read((uint8_t *)&_prefs.telemetry_mode_env, sizeof(_prefs.telemetry_mode_env));   // 71
    file.read((uint8_t *)&_prefs.rx_delay_base, sizeof(_prefs.rx_delay_base));             // 72
    file.read((uint8_t *)&_prefs.advert_loc_policy, sizeof(_prefs.advert_loc_policy));     // 76
    file.read((uint8_t *)&_prefs.multi_acks, sizeof(_prefs.multi_acks));                   // 77
    file.read(pad, 2);                                                                     // 78
    file.read((uint8_t *)&_prefs.ble_pin, sizeof(_prefs.ble_pin));                         // 80
    file.read((uint8_t *)&_prefs.buzzer_quiet, sizeof(_prefs.buzzer_quiet));               // 84
    file.read((uint8_t *)&_prefs.gps_enabled, sizeof(_prefs.gps_enabled));                 // 85
    file.read(pad, 2);                                                                     // 86
    file.read((uint8_t *)&_prefs.screen_timeout_seconds, sizeof(_prefs.screen_timeout_seconds)); // 88

    file.close();
  }
}

void DataStore::savePrefs(const NodePrefs& _prefs, double node_lat, double node_lon) {
  File file = openWrite(_fs, "/new_prefs");
  if (file) {
    uint8_t pad[8];
    memset(pad, 0, sizeof(pad));

    file.write((uint8_t *)&_prefs.airtime_factor, sizeof(float));                           // 0
    file.write((uint8_t *)_prefs.node_name, sizeof(_prefs.node_name));                      // 4
    file.write(pad, 4);                                                                     // 36
    file.write((uint8_t *)&node_lat, sizeof(node_lat));                                     // 40
    file.write((uint8_t *)&node_lon, sizeof(node_lon));                                     // 48
    file.write((uint8_t *)&_prefs.freq, sizeof(_prefs.freq));                               // 56
    file.write((uint8_t *)&_prefs.sf, sizeof(_prefs.sf));                                   // 60
    file.write((uint8_t *)&_prefs.cr, sizeof(_prefs.cr));                                   // 61
    file.write(pad, 1);                                                                     // 62
    file.write((uint8_t *)&_prefs.manual_add_contacts, sizeof(_prefs.manual_add_contacts)); // 63
    file.write((uint8_t *)&_prefs.bw, sizeof(_prefs.bw));                                   // 64
    file.write((uint8_t *)&_prefs.tx_power_dbm, sizeof(_prefs.tx_power_dbm));               // 68
    file.write((uint8_t *)&_prefs.telemetry_mode_base, sizeof(_prefs.telemetry_mode_base)); // 69
    file.write((uint8_t *)&_prefs.telemetry_mode_loc, sizeof(_prefs.telemetry_mode_loc));   // 70
    file.write((uint8_t *)&_prefs.telemetry_mode_env, sizeof(_prefs.telemetry_mode_env));   // 71
    file.write((uint8_t *)&_prefs.rx_delay_base, sizeof(_prefs.rx_delay_base));             // 72
    file.write((uint8_t *)&_prefs.advert_loc_policy, sizeof(_prefs.advert_loc_policy));     // 76
    file.write((uint8_t *)&_prefs.multi_acks, sizeof(_prefs.multi_acks));                   // 77
    file.write(pad, 2);                                                                     // 78
    file.write((uint8_t *)&_prefs.ble_pin, sizeof(_prefs.ble_pin));                         // 80
    file.write((uint8_t *)&_prefs.buzzer_quiet, sizeof(_prefs.buzzer_quiet));               // 84
    file.write((uint8_t *)&_prefs.gps_enabled, sizeof(_prefs.gps_enabled));                 // 85
    file.write(pad, 2);                                                                     // 86
    file.write((uint8_t *)&_prefs.screen_timeout_seconds, sizeof(_prefs.screen_timeout_seconds)); // 88

    file.close();
  }
}

#if defined(ESP32)
namespace {
constexpr size_t MAX_PREFS_FILE_SIZE = 512;
constexpr uint8_t PREFS_BACKUP_MAGIC[4] = {'M', 'C', 'P', 'S'};
constexpr uint16_t PREFS_BACKUP_VERSION = 1;

uint32_t prefsCrc32(const uint8_t* data, size_t length) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
  }
  return ~crc;
}

bool readExact(File& file, uint8_t* buffer, size_t length) {
  return file.read(buffer, length) == length;
}
}

DataStore::PrefsBackupResult DataStore::backupPrefs(FILESYSTEM& destination, const char* filename) {
  File source = openRead(_fs, "/new_prefs");
  if (!source) return PrefsBackupResult::SOURCE_NOT_FOUND;
  const size_t payload_size = source.size();
  if (payload_size == 0 || payload_size > MAX_PREFS_FILE_SIZE) {
    source.close();
    return PrefsBackupResult::READ_FAILED;
  }

  uint8_t payload[MAX_PREFS_FILE_SIZE];
  if (!readExact(source, payload, payload_size)) {
    source.close();
    return PrefsBackupResult::READ_FAILED;
  }
  source.close();

  File backup = destination.open(filename, "w");
  if (!backup) return PrefsBackupResult::WRITE_FAILED;

  const uint16_t payload_length = payload_size;
  const uint32_t crc = prefsCrc32(payload, payload_size);
  bool success = backup.write(PREFS_BACKUP_MAGIC, sizeof(PREFS_BACKUP_MAGIC)) == sizeof(PREFS_BACKUP_MAGIC);
  success = success && backup.write(reinterpret_cast<const uint8_t*>(&PREFS_BACKUP_VERSION), sizeof(PREFS_BACKUP_VERSION)) == sizeof(PREFS_BACKUP_VERSION);
  success = success && backup.write(reinterpret_cast<const uint8_t*>(&payload_length), sizeof(payload_length)) == sizeof(payload_length);
  success = success && backup.write(reinterpret_cast<const uint8_t*>(&crc), sizeof(crc)) == sizeof(crc);
  success = success && backup.write(payload, payload_size) == payload_size;
  backup.flush();
  backup.close();

  if (!success) {
    destination.remove(filename);
    return PrefsBackupResult::WRITE_FAILED;
  }
  return PrefsBackupResult::OK;
}

DataStore::PrefsBackupResult DataStore::restorePrefs(FILESYSTEM& source, const char* filename) {
  File backup = source.open(filename, "r");
  if (!backup) return PrefsBackupResult::SOURCE_NOT_FOUND;

  uint8_t magic[sizeof(PREFS_BACKUP_MAGIC)];
  uint16_t version = 0;
  uint16_t payload_length = 0;
  uint32_t stored_crc = 0;
  uint8_t payload[MAX_PREFS_FILE_SIZE];

  bool success = readExact(backup, magic, sizeof(magic));
  success = success && readExact(backup, reinterpret_cast<uint8_t*>(&version), sizeof(version));
  success = success && readExact(backup, reinterpret_cast<uint8_t*>(&payload_length), sizeof(payload_length));
  success = success && readExact(backup, reinterpret_cast<uint8_t*>(&stored_crc), sizeof(stored_crc));
  if (!success || memcmp(magic, PREFS_BACKUP_MAGIC, sizeof(magic)) != 0 ||
      version != PREFS_BACKUP_VERSION || payload_length == 0 || payload_length > sizeof(payload) ||
      backup.size() != sizeof(magic) + sizeof(version) + sizeof(payload_length) + sizeof(stored_crc) + payload_length) {
    backup.close();
    return PrefsBackupResult::INVALID_BACKUP;
  }
  success = readExact(backup, payload, payload_length);
  backup.close();
  if (!success || prefsCrc32(payload, payload_length) != stored_crc) {
    return PrefsBackupResult::INVALID_BACKUP;
  }

  const char* temporary = "/new_prefs.tmp";
  const char* previous = "/new_prefs.bak";
  if (_fs->exists(temporary)) _fs->remove(temporary);
  File restored = openWrite(_fs, temporary);
  if (!restored) return PrefsBackupResult::WRITE_FAILED;
  success = restored.write(payload, payload_length) == payload_length;
  restored.flush();
  restored.close();
  if (!success) {
    if (_fs->exists(temporary)) _fs->remove(temporary);
    return PrefsBackupResult::WRITE_FAILED;
  }

  if (_fs->exists(previous)) _fs->remove(previous);
  bool had_current = _fs->exists("/new_prefs");
  if (had_current && !_fs->rename("/new_prefs", previous)) {
    if (_fs->exists(temporary)) _fs->remove(temporary);
    return PrefsBackupResult::WRITE_FAILED;
  }
  if (!_fs->rename(temporary, "/new_prefs")) {
    if (had_current) _fs->rename(previous, "/new_prefs");
    if (_fs->exists(temporary)) _fs->remove(temporary);
    return PrefsBackupResult::WRITE_FAILED;
  }
  if (_fs->exists(previous)) _fs->remove(previous);
  return PrefsBackupResult::OK;
}

#if defined(M5STACK_CARDPUTER)
namespace {
constexpr uint8_t FULL_BACKUP_MAGIC[4] = {'M', 'C', 'F', 'B'};
constexpr uint16_t FULL_BACKUP_VERSION = 1;
constexpr uint32_t FULL_BACKUP_PBKDF2_ITERATIONS = 100000;
constexpr size_t FULL_BACKUP_CHUNK = 1024;
constexpr size_t FULL_BACKUP_TAG_SIZE = 16;

#pragma pack(push, 1)
struct FullBackupHeader {
  uint8_t magic[4];
  uint16_t version;
  uint16_t header_size;
  uint32_t iterations;
  uint32_t partition_size;
  uint32_t plaintext_size;
  uint8_t salt[16];
  uint8_t nonce[12];
};

struct FullBackupPrefix {
  uint8_t magic[4];
  uint8_t version;
  uint8_t brightness;
  uint8_t main_color;
  uint8_t secondary_color;
  uint8_t reserved[8];
};
#pragma pack(pop)

static_assert(sizeof(FullBackupHeader) == 48, "Unexpected backup header size");
static_assert(sizeof(FullBackupPrefix) == 16, "Unexpected backup prefix size");

void secureClear(void* data, size_t length) {
  volatile uint8_t* bytes = static_cast<volatile uint8_t*>(data);
  while (length--) *bytes++ = 0;
}

bool constantTimeEqual(const uint8_t* left, const uint8_t* right, size_t length) {
  uint8_t difference = 0;
  for (size_t i = 0; i < length; ++i) difference |= left[i] ^ right[i];
  return difference == 0;
}

bool deriveBackupKey(const char* passphrase, const FullBackupHeader& header, uint8_t key[32]) {
  if (!passphrase || !passphrase[0]) return false;
  const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (!info) return false;
  mbedtls_md_context_t md;
  mbedtls_md_init(&md);
  int result = mbedtls_md_setup(&md, info, 1);
  if (result == 0) {
    result = mbedtls_pkcs5_pbkdf2_hmac(
      &md, reinterpret_cast<const uint8_t*>(passphrase), strlen(passphrase),
      header.salt, sizeof(header.salt), header.iterations, 32, key);
  }
  mbedtls_md_free(&md);
  return result == 0;
}

bool readUIBackupPrefix(FullBackupPrefix& prefix) {
  memcpy(prefix.magic, "MCUI", 4);
  prefix.version = 1;
  memset(prefix.reserved, 0, sizeof(prefix.reserved));
  Preferences preferences;
  if (!preferences.begin("ui_settings", true)) return false;
  prefix.brightness = preferences.getUChar("brightness", 128);
  prefix.main_color = preferences.getUChar("main_color", 0);
  prefix.secondary_color = preferences.getUChar("sec_color", 1);
  preferences.end();
  return true;
}

bool writeUIBackupPrefix(const FullBackupPrefix& prefix) {
  Preferences preferences;
  if (!preferences.begin("ui_settings", false)) return false;
  bool success = preferences.putUChar("brightness", prefix.brightness) == 1;
  success = success && preferences.putUChar("main_color", prefix.main_color) == 1;
  success = success && preferences.putUChar("sec_color", prefix.secondary_color) == 1;
  preferences.end();
  return success;
}

const esp_partition_t* findSPIFFSPartition() {
  return esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, nullptr);
}

bool validFullBackupHeader(const FullBackupHeader& header, size_t file_size, uint32_t partition_size) {
  return memcmp(header.magic, FULL_BACKUP_MAGIC, sizeof(header.magic)) == 0 &&
    header.version == FULL_BACKUP_VERSION && header.header_size == sizeof(header) &&
    header.iterations >= 10000 && header.iterations <= 1000000 &&
    header.partition_size == partition_size &&
    header.plaintext_size == sizeof(FullBackupPrefix) + partition_size &&
    file_size == sizeof(header) + header.plaintext_size + FULL_BACKUP_TAG_SIZE;
}

DataStore::FullBackupResult authenticateFullBackup(
    FILESYSTEM& source, const char* filename, const esp_partition_t* partition,
    const char* passphrase, FullBackupHeader& header, FullBackupPrefix& prefix,
    uint8_t key[32]) {
  File input = source.open(filename, "r");
  if (!input) return DataStore::FullBackupResult::SOURCE_NOT_FOUND;
  if (!readExact(input, reinterpret_cast<uint8_t*>(&header), sizeof(header))) {
    input.close();
    return DataStore::FullBackupResult::READ_FAILED;
  }
  if (memcmp(header.magic, FULL_BACKUP_MAGIC, sizeof(header.magic)) != 0 ||
      header.version != FULL_BACKUP_VERSION || header.header_size != sizeof(header)) {
    input.close();
    return DataStore::FullBackupResult::INVALID_BACKUP;
  }
  if (header.partition_size != partition->size) {
    input.close();
    return DataStore::FullBackupResult::INCOMPATIBLE_BACKUP;
  }
  if (!validFullBackupHeader(header, input.size(), partition->size)) {
    input.close();
    return DataStore::FullBackupResult::INVALID_BACKUP;
  }
  if (!deriveBackupKey(passphrase, header, key)) {
    input.close();
    return DataStore::FullBackupResult::AUTH_FAILED;
  }

  mbedtls_gcm_context gcm;
  mbedtls_gcm_init(&gcm);
  int crypto_result = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);
  if (crypto_result == 0) {
    crypto_result = mbedtls_gcm_starts(&gcm, MBEDTLS_GCM_DECRYPT, header.nonce,
      sizeof(header.nonce), reinterpret_cast<const uint8_t*>(&header), sizeof(header));
  }

  uint8_t encrypted[FULL_BACKUP_CHUNK];
  uint8_t plaintext[FULL_BACKUP_CHUNK];
  uint32_t remaining = header.plaintext_size;
  bool got_prefix = false;
  while (crypto_result == 0 && remaining > 0) {
    size_t length = min(static_cast<uint32_t>(sizeof(encrypted)), remaining);
    if (!readExact(input, encrypted, length)) {
      crypto_result = -1;
      break;
    }
    crypto_result = mbedtls_gcm_update(&gcm, length, encrypted, plaintext);
    if (!got_prefix && crypto_result == 0) {
      memcpy(&prefix, plaintext, sizeof(prefix));
      got_prefix = true;
    }
    remaining -= length;
  }
  uint8_t calculated_tag[FULL_BACKUP_TAG_SIZE];
  uint8_t stored_tag[FULL_BACKUP_TAG_SIZE];
  if (crypto_result == 0) crypto_result = mbedtls_gcm_finish(&gcm, calculated_tag, sizeof(calculated_tag));
  bool tag_read = readExact(input, stored_tag, sizeof(stored_tag));
  input.close();
  mbedtls_gcm_free(&gcm);
  secureClear(encrypted, sizeof(encrypted));
  secureClear(plaintext, sizeof(plaintext));

  if (crypto_result != 0 || !tag_read || !constantTimeEqual(calculated_tag, stored_tag, sizeof(stored_tag))) {
    secureClear(calculated_tag, sizeof(calculated_tag));
    secureClear(stored_tag, sizeof(stored_tag));
    secureClear(key, 32);
    return DataStore::FullBackupResult::AUTH_FAILED;
  }
  secureClear(calculated_tag, sizeof(calculated_tag));
  secureClear(stored_tag, sizeof(stored_tag));
  if (!got_prefix || memcmp(prefix.magic, "MCUI", 4) != 0 || prefix.version != 1) {
    secureClear(key, 32);
    return DataStore::FullBackupResult::INVALID_BACKUP;
  }
  return DataStore::FullBackupResult::OK;
}
}

DataStore::FullBackupResult DataStore::backupFullEncrypted(
    FILESYSTEM& destination, const char* filename, const char* passphrase) {
  const esp_partition_t* partition = findSPIFFSPartition();
  if (!partition) return FullBackupResult::READ_FAILED;

  FullBackupHeader header{};
  memcpy(header.magic, FULL_BACKUP_MAGIC, sizeof(header.magic));
  header.version = FULL_BACKUP_VERSION;
  header.header_size = sizeof(header);
  header.iterations = FULL_BACKUP_PBKDF2_ITERATIONS;
  header.partition_size = partition->size;
  header.plaintext_size = sizeof(FullBackupPrefix) + partition->size;
  esp_fill_random(header.salt, sizeof(header.salt));
  esp_fill_random(header.nonce, sizeof(header.nonce));

  uint8_t key[32];
  if (!deriveBackupKey(passphrase, header, key)) return FullBackupResult::WRITE_FAILED;
  FullBackupPrefix prefix{};
  if (!readUIBackupPrefix(prefix)) {
    secureClear(key, sizeof(key));
    return FullBackupResult::READ_FAILED;
  }

  String temporary = String(filename) + ".tmp";
  String previous = String(filename) + ".bak";
  if (destination.exists(temporary)) destination.remove(temporary);
  File output = destination.open(temporary, "w");
  if (!output) {
    secureClear(key, sizeof(key));
    return FullBackupResult::WRITE_FAILED;
  }

  bool success = output.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header)) == sizeof(header);
  mbedtls_gcm_context gcm;
  mbedtls_gcm_init(&gcm);
  int crypto_result = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);
  if (crypto_result == 0) {
    crypto_result = mbedtls_gcm_starts(&gcm, MBEDTLS_GCM_ENCRYPT, header.nonce,
      sizeof(header.nonce), reinterpret_cast<const uint8_t*>(&header), sizeof(header));
  }

  uint8_t encrypted[FULL_BACKUP_CHUNK];
  crypto_result = crypto_result == 0
    ? mbedtls_gcm_update(&gcm, sizeof(prefix), reinterpret_cast<const uint8_t*>(&prefix), encrypted)
    : crypto_result;
  success = success && crypto_result == 0 && output.write(encrypted, sizeof(prefix)) == sizeof(prefix);

  uint8_t plaintext[FULL_BACKUP_CHUNK];
  for (uint32_t offset = 0; success && offset < partition->size; offset += sizeof(plaintext)) {
    size_t length = min(static_cast<uint32_t>(sizeof(plaintext)), partition->size - offset);
    if (esp_partition_read(partition, offset, plaintext, length) != ESP_OK ||
        mbedtls_gcm_update(&gcm, length, plaintext, encrypted) != 0 ||
        output.write(encrypted, length) != length) {
      success = false;
    }
  }
  uint8_t tag[FULL_BACKUP_TAG_SIZE];
  if (success && mbedtls_gcm_finish(&gcm, tag, sizeof(tag)) == 0) {
    success = output.write(tag, sizeof(tag)) == sizeof(tag);
  } else {
    success = false;
  }
  output.flush();
  output.close();
  mbedtls_gcm_free(&gcm);
  secureClear(key, sizeof(key));
  secureClear(plaintext, sizeof(plaintext));
  secureClear(encrypted, sizeof(encrypted));
  secureClear(tag, sizeof(tag));

  if (!success) {
    if (destination.exists(temporary)) destination.remove(temporary);
    return FullBackupResult::WRITE_FAILED;
  }
  if (destination.exists(previous)) destination.remove(previous);
  bool had_current = destination.exists(filename);
  if (had_current && !destination.rename(filename, previous)) {
    destination.remove(temporary);
    return FullBackupResult::WRITE_FAILED;
  }
  if (!destination.rename(temporary, filename)) {
    if (had_current) destination.rename(previous, filename);
    destination.remove(temporary);
    return FullBackupResult::WRITE_FAILED;
  }
  if (destination.exists(previous)) destination.remove(previous);
  return FullBackupResult::OK;
}

DataStore::FullBackupResult DataStore::restoreFullEncrypted(
    FILESYSTEM& source, const char* filename, const char* passphrase) {
  const esp_partition_t* partition = findSPIFFSPartition();
  if (!partition) return FullBackupResult::WRITE_FAILED;

  FullBackupHeader header{};
  FullBackupPrefix prefix{};
  uint8_t key[32];
  FullBackupResult authenticated = authenticateFullBackup(
    source, filename, partition, passphrase, header, prefix, key);
  if (authenticated != FullBackupResult::OK) return authenticated;

  File input = source.open(filename, "r");
  if (!input || !input.seek(sizeof(header))) {
    if (input) input.close();
    secureClear(key, sizeof(key));
    return FullBackupResult::READ_FAILED;
  }
  mbedtls_gcm_context gcm;
  mbedtls_gcm_init(&gcm);
  int crypto_result = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);
  if (crypto_result == 0) {
    crypto_result = mbedtls_gcm_starts(&gcm, MBEDTLS_GCM_DECRYPT, header.nonce,
      sizeof(header.nonce), reinterpret_cast<const uint8_t*>(&header), sizeof(header));
  }

  uint8_t encrypted[FULL_BACKUP_CHUNK];
  uint8_t plaintext[FULL_BACKUP_CHUNK];
  if (!readExact(input, encrypted, sizeof(prefix)) ||
      mbedtls_gcm_update(&gcm, sizeof(prefix), encrypted, plaintext) != 0) {
    crypto_result = -1;
  }

  // Authentication above completed before this destructive point.
  SPIFFS.end();
  if (crypto_result == 0 && esp_partition_erase_range(partition, 0, partition->size) != ESP_OK) {
    crypto_result = -1;
  }
  for (uint32_t offset = 0; crypto_result == 0 && offset < partition->size; offset += sizeof(plaintext)) {
    size_t length = min(static_cast<uint32_t>(sizeof(plaintext)), partition->size - offset);
    if (!readExact(input, encrypted, length) ||
        mbedtls_gcm_update(&gcm, length, encrypted, plaintext) != 0 ||
        esp_partition_write(partition, offset, plaintext, length) != ESP_OK) {
      crypto_result = -1;
    }
  }
  uint8_t calculated_tag[FULL_BACKUP_TAG_SIZE];
  uint8_t stored_tag[FULL_BACKUP_TAG_SIZE];
  if (crypto_result == 0) crypto_result = mbedtls_gcm_finish(&gcm, calculated_tag, sizeof(calculated_tag));
  bool tag_read = readExact(input, stored_tag, sizeof(stored_tag));
  input.close();
  mbedtls_gcm_free(&gcm);
  bool success = crypto_result == 0 && tag_read &&
    constantTimeEqual(calculated_tag, stored_tag, sizeof(stored_tag)) && writeUIBackupPrefix(prefix);
  secureClear(key, sizeof(key));
  secureClear(plaintext, sizeof(plaintext));
  secureClear(encrypted, sizeof(encrypted));
  secureClear(calculated_tag, sizeof(calculated_tag));
  secureClear(stored_tag, sizeof(stored_tag));
  return success ? FullBackupResult::OK : FullBackupResult::WRITE_FAILED;
}
#endif
#endif

void DataStore::loadContacts(DataStoreHost* host) {
File file = openRead(_getContactsChannelsFS(), "/contacts3");
    if (file) {
      bool full = false;
      while (!full) {
        ContactInfo c;
        uint8_t pub_key[32];
        uint8_t unused;

        bool success = (file.read(pub_key, 32) == 32);
        success = success && (file.read((uint8_t *)&c.name, 32) == 32);
        success = success && (file.read(&c.type, 1) == 1);
        success = success && (file.read(&c.flags, 1) == 1);
        success = success && (file.read(&unused, 1) == 1);
        success = success && (file.read((uint8_t *)&c.sync_since, 4) == 4); // was 'reserved'
        success = success && (file.read((uint8_t *)&c.out_path_len, 1) == 1);
        success = success && (file.read((uint8_t *)&c.last_advert_timestamp, 4) == 4);
        success = success && (file.read(c.out_path, 64) == 64);
        success = success && (file.read((uint8_t *)&c.lastmod, 4) == 4);
        success = success && (file.read((uint8_t *)&c.gps_lat, 4) == 4);
        success = success && (file.read((uint8_t *)&c.gps_lon, 4) == 4);

        if (!success) break; // EOF

        c.id = mesh::Identity(pub_key);
        if (!host->onContactLoaded(c)) full = true;
      }
      file.close();
    }
}

void DataStore::saveContacts(DataStoreHost* host) {
  File file = openWrite(_getContactsChannelsFS(), "/contacts3");
  if (file) {
    uint32_t idx = 0;
    ContactInfo c;
    uint8_t unused = 0;

    while (host->getContactForSave(idx, c)) {
      bool success = (file.write(c.id.pub_key, 32) == 32);
      success = success && (file.write((uint8_t *)&c.name, 32) == 32);
      success = success && (file.write(&c.type, 1) == 1);
      success = success && (file.write(&c.flags, 1) == 1);
      success = success && (file.write(&unused, 1) == 1);
      success = success && (file.write((uint8_t *)&c.sync_since, 4) == 4);
      success = success && (file.write((uint8_t *)&c.out_path_len, 1) == 1);
      success = success && (file.write((uint8_t *)&c.last_advert_timestamp, 4) == 4);
      success = success && (file.write(c.out_path, 64) == 64);
      success = success && (file.write((uint8_t *)&c.lastmod, 4) == 4);
      success = success && (file.write((uint8_t *)&c.gps_lat, 4) == 4);
      success = success && (file.write((uint8_t *)&c.gps_lon, 4) == 4);

      if (!success) break; // write failed

      idx++;  // advance to next contact
    }
    file.close();
  }
}

void DataStore::loadChannels(DataStoreHost* host) {
    File file = openRead(_getContactsChannelsFS(), "/channels2");
    if (file) {
      bool full = false;
      uint8_t channel_idx = 0;
      while (!full) {
        ChannelDetails ch;
        uint8_t unused[4];

        bool success = (file.read(unused, 4) == 4);
        success = success && (file.read((uint8_t *)ch.name, 32) == 32);
        success = success && (file.read((uint8_t *)ch.channel.secret, 32) == 32);

        if (!success) break; // EOF

        if (host->onChannelLoaded(channel_idx, ch)) {
          channel_idx++;
        } else {
          full = true;
        }
      }
      file.close();
    }
}

void DataStore::saveChannels(DataStoreHost* host) {
  File file = openWrite(_getContactsChannelsFS(), "/channels2");
  if (file) {
    uint8_t channel_idx = 0;
    ChannelDetails ch;
    uint8_t unused[4];
    memset(unused, 0, 4);

    while (host->getChannelForSave(channel_idx, ch)) {
      bool success = (file.write(unused, 4) == 4);
      success = success && (file.write((uint8_t *)ch.name, 32) == 32);
      success = success && (file.write((uint8_t *)ch.channel.secret, 32) == 32);

      if (!success) break; // write failed
      channel_idx++;
    }
    file.close();
  }
}

#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)

#define MAX_ADVERT_PKT_LEN   (2 + 32 + PUB_KEY_SIZE + 4 + SIGNATURE_SIZE + MAX_ADVERT_DATA_SIZE)

struct BlobRec {
  uint32_t timestamp;
  uint8_t  key[7];
  uint8_t  len;
  uint8_t  data[MAX_ADVERT_PKT_LEN];
};

void DataStore::checkAdvBlobFile() {
  if (!_getContactsChannelsFS()->exists("/adv_blobs")) {
    File file = openWrite(_getContactsChannelsFS(), "/adv_blobs");
    if (file) {
      BlobRec zeroes;
      memset(&zeroes, 0, sizeof(zeroes));
      for (int i = 0; i < MAX_BLOBRECS; i++) {     // pre-allocate to fixed size
        file.write((uint8_t *) &zeroes, sizeof(zeroes));
      }
      file.close();
    }
  }
}

void DataStore::migrateToSecondaryFS() {
  // migrate old adv_blobs, contacts3 and channels2 files to secondary FS if they don't already exist
  if (!_fsExtra->exists("/adv_blobs")) {
    if (_fs->exists("/adv_blobs")) {
    File oldAdvBlobs = openRead(_fs, "/adv_blobs");
    File newAdvBlobs = openWrite(_fsExtra, "/adv_blobs");

    if (oldAdvBlobs && newAdvBlobs) {
      BlobRec rec;
      size_t count = 0;

      // Copy 20 BlobRecs from old to new
      while (count < 20 && oldAdvBlobs.read((uint8_t *)&rec, sizeof(rec)) == sizeof(rec)) {
        newAdvBlobs.seek(count * sizeof(BlobRec));
        newAdvBlobs.write((uint8_t *)&rec, sizeof(rec));
        count++;
      }
    }
    if (oldAdvBlobs) oldAdvBlobs.close();
    if (newAdvBlobs) newAdvBlobs.close();
    _fs->remove("/adv_blobs");
    }
  }
  if (!_fsExtra->exists("/contacts3")) {
    if (_fs->exists("/contacts3")) {
      File oldFile = openRead(_fs, "/contacts3");
      File newFile = openWrite(_fsExtra, "/contacts3");

      if (oldFile && newFile) {
        uint8_t buf[64];
        int n;
        while ((n = oldFile.read(buf, sizeof(buf))) > 0) {
          newFile.write(buf, n);
        }
      }
      if (oldFile) oldFile.close();
      if (newFile) newFile.close();
      _fs->remove("/contacts3");
    }
  }
  if (!_fsExtra->exists("/channels2")) {
    if (_fs->exists("/channels2")) {
      File oldFile = openRead(_fs, "/channels2");
      File newFile = openWrite(_fsExtra, "/channels2");

      if (oldFile && newFile) {
        uint8_t buf[64];
        int n;
        while ((n = oldFile.read(buf, sizeof(buf))) > 0) {
          newFile.write(buf, n);
        }
      }
      if (oldFile) oldFile.close();
      if (newFile) newFile.close();
      _fs->remove("/channels2");
    }
  }
  // cleanup nodes which have been testing the extra fs, copy _main.id and new_prefs back to primary
  if (_fsExtra->exists("/_main.id")) {
      if (_fs->exists("/_main.id")) {_fs->remove("/_main.id");}
      File oldFile = openRead(_fsExtra, "/_main.id");
      File newFile = openWrite(_fs, "/_main.id");

      if (oldFile && newFile) {
        uint8_t buf[64];
        int n;
        while ((n = oldFile.read(buf, sizeof(buf))) > 0) {
          newFile.write(buf, n);
        }
      }
      if (oldFile) oldFile.close();
      if (newFile) newFile.close();
      _fsExtra->remove("/_main.id");
  }
  if (_fsExtra->exists("/new_prefs")) {
    if (_fs->exists("/new_prefs")) {_fs->remove("/new_prefs");}
      File oldFile = openRead(_fsExtra, "/new_prefs");
      File newFile = openWrite(_fs, "/new_prefs");

      if (oldFile && newFile) {
        uint8_t buf[64];
        int n;
        while ((n = oldFile.read(buf, sizeof(buf))) > 0) {
          newFile.write(buf, n);
        }
      }
      if (oldFile) oldFile.close();
      if (newFile) newFile.close();
      _fsExtra->remove("/new_prefs");
  }
  // remove files from where they should not be anymore
  if (_fs->exists("/adv_blobs")) {
    _fs->remove("/adv_blobs");
  }
  if (_fs->exists("/contacts3")) {
    _fs->remove("/contacts3");
  }
  if (_fs->exists("/channels2")) {
    _fs->remove("/channels2");
  }
  if (_fsExtra->exists("/_main.id")) {
    _fsExtra->remove("/_main.id");
  }
  if (_fsExtra->exists("/new_prefs")) {
    _fsExtra->remove("/new_prefs");
  }
}

uint8_t DataStore::getBlobByKey(const uint8_t key[], int key_len, uint8_t dest_buf[]) {
  File file = openRead(_getContactsChannelsFS(), "/adv_blobs");
  uint8_t len = 0;  // 0 = not found
  if (file) {
    BlobRec tmp;
    while (file.read((uint8_t *) &tmp, sizeof(tmp)) == sizeof(tmp)) {
      if (memcmp(key, tmp.key, sizeof(tmp.key)) == 0) {  // only match by 7 byte prefix
        len = tmp.len;
        memcpy(dest_buf, tmp.data, len);
        break;
      }
    }
    file.close();
  }
  return len;
}

bool DataStore::putBlobByKey(const uint8_t key[], int key_len, const uint8_t src_buf[], uint8_t len) {
  if (len < PUB_KEY_SIZE+4+SIGNATURE_SIZE || len > MAX_ADVERT_PKT_LEN) return false;
  checkAdvBlobFile();
  File file = _getContactsChannelsFS()->open("/adv_blobs", FILE_O_WRITE);
  if (file) {
    uint32_t pos = 0, found_pos = 0;
    uint32_t min_timestamp = 0xFFFFFFFF;

    // search for matching key OR evict by oldest timestmap
    BlobRec tmp;
    file.seek(0);
    while (file.read((uint8_t *) &tmp, sizeof(tmp)) == sizeof(tmp)) {
      if (memcmp(key, tmp.key, sizeof(tmp.key)) == 0) {  // only match by 7 byte prefix
        found_pos = pos;
        break;
      }
      if (tmp.timestamp < min_timestamp) {
        min_timestamp = tmp.timestamp;
        found_pos = pos;
      }

      pos += sizeof(tmp);
    }

    memcpy(tmp.key, key, sizeof(tmp.key));  // just record 7 byte prefix of key
    memcpy(tmp.data, src_buf, len);
    tmp.len = len;
    tmp.timestamp = _clock->getCurrentTime();

    file.seek(found_pos);
    file.write((uint8_t *) &tmp, sizeof(tmp));

    file.close();
    return true;
  }
  return false; // error
}
#else
uint8_t DataStore::getBlobByKey(const uint8_t key[], int key_len, uint8_t dest_buf[]) {
  char path[64];
  char fname[18];

  if (key_len > 8) key_len = 8; // just use first 8 bytes (prefix)
  mesh::Utils::toHex(fname, key, key_len);
  sprintf(path, "/bl/%s", fname);

  if (_fs->exists(path)) {
    File f = openRead(_fs, path);
    if (f) {
      int len = f.read(dest_buf, 255); // currently MAX 255 byte blob len supported!!
      f.close();
      return len;
    }
  }
  return 0; // not found
}

bool DataStore::putBlobByKey(const uint8_t key[], int key_len, const uint8_t src_buf[], uint8_t len) {
  char path[64];
  char fname[18];

  if (key_len > 8) key_len = 8; // just use first 8 bytes (prefix)
  mesh::Utils::toHex(fname, key, key_len);
  sprintf(path, "/bl/%s", fname);

  File f = openWrite(_fs, path);
  if (f) {
    int n = f.write(src_buf, len);
    f.close();
    if (n == len) return true; // success!

    _fs->remove(path); // blob was only partially written!
  }
  return false; // error
}
#endif
