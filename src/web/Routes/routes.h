#ifndef ROUTES_H
#define ROUTES_H

#include <MVCFramework.h>
#include <LittleFS.h>
#include "Routing/Router.h"
#include "web/Controllers/AuthController.h"
#include "web/Controllers/SystemController.h"

struct {
  bool authenticated = false;
} sessions[5];

void registerWebRoutes(Router* router);
void registerApiRoutes(Router* router);
void registerWebSocketRoutes(Router* router);

#endif
