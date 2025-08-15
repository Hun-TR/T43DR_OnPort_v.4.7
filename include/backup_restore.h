#ifndef BACKUP_RESTORE_H
#define BACKUP_RESTORE_H

#include <Arduino.h>

// Function declarations
String exportSettingsToJSON();
bool importSettingsFromJSON(const String& jsonData);
bool saveBackupToFile(const String& filename);
bool loadBackupFromFile(const String& filename);
void handleBackupDownload();
void handleBackupUpload();
void createAutomaticBackup();

#endif // BACKUP_RESTORE_H