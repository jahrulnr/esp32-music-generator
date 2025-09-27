#include "app/setup.h"

FTPServer ftpSrv(LittleFS);

void setupFTPServer() {
	ftpSrv.begin("jahrulnr", "cozmo");
}