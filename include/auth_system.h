#ifndef AUTH_SYSTEM_H
#define AUTH_SYSTEM_H

#include <Arduino.h>

bool checkSession();
void handleUserLogin();
void handleUserLogout();
void refreshSession();

#endif

