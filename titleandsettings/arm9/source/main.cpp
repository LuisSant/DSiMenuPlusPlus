/*-----------------------------------------------------------------
 Copyright (C) 2005 - 2013
	Michael "Chishm" Chisholm
	Dave "WinterMute" Murphy
	Claudio "sverx"

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

------------------------------------------------------------------*/
#include <nds.h>
#include <stdio.h>
#include <fat.h>
#include <sys/stat.h>
#include <limits.h>

#include <string.h>
#include <unistd.h>
#include <maxmod9.h>
#include <gl2d.h>

#include "autoboot.h"

#include "graphics/graphics.h"

#include "nds_loader_arm9.h"

#include "graphics/fontHandler.h"

#include "inifile.h"

#include "soundbank.h"
#include "soundbank_bin.h"

#include "sr_data_srllastran.h"	// For rebooting into the game (NTR-mode touch screen)
#include "sr_data_srllastran_twltouch.h"	// For rebooting into the game (TWL-mode touch screen)

bool renderScreens = false;
bool fadeType = false;		// false = out, true = in

const char* settingsinipath = "/_nds/dsimenuplusplus/settings.ini";
const char* hiyacfwinipath = "sd:/hiya/settings.ini";
const char* bootstrapinipath = "sd:/_nds/nds-bootstrap.ini";

std::string dsiWareSrlPath;
std::string dsiWarePubPath;
std::string dsiWarePrvPath;
std::string homebrewArg;
std::string bootstrapfilename;

static int consoleModel = 0;
/*	0 = Nintendo DSi (Retail)
	1 = Nintendo DSi (Dev/Panda)
	2 = Nintendo 3DS
	3 = New Nintendo 3DS	*/

static bool showlogo = true;
static bool gotosettings = false;

const char* romreadled_valuetext;

static int bstrap_loadingScreen = 1;

static int donorSdkVer = 0;

static int launchType = 1;	// 0 = Slot-1, 1 = SD/Flash card, 2 = DSiWare, 3 = NES, 4 = (S)GB(C)
static bool resetSlot1 = true;
static bool bootstrapFile = false;
static bool homebrewBootstrap = false;

static bool useGbarunner = false;
static bool autorun = false;
static int theme = 0;
static int subtheme = 0;
static bool showDirectories = true;
static bool showBoxArt = true;
static bool animateDsiIcons = false;

static int bstrap_language = -1;
static bool boostCpu = false;	// false == NTR, true == TWL
static bool bstrap_debug = false;
static int bstrap_romreadled = 0;
//static bool bstrap_lockARM9scfgext = false;

static bool soundfreq = false;	// false == 32.73 kHz, true == 47.61 kHz

bool flashcardUsed = false;

static int flashcard;
/* Flashcard value
	0: DSTT/R4i Gold/R4i-SDHC/R4 SDHC Dual-Core/R4 SDHC Upgrade/SC DSONE
	1: R4DS (Original Non-SDHC version)/ M3 Simply
	2: R4iDSN/R4i Gold RTS/R4 Ultra
	3: Acekard 2(i)/Galaxy Eagle/M3DS Real
	4: Acekard RPG
	5: Ace 3DS+/Gateway Blue Card/R4iTT
	6: SuperCard DSTWO
*/

void LoadSettings(void) {
	// GUI
	CIniFile settingsini( settingsinipath );

	useGbarunner = settingsini.GetInt("SRLOADER", "USE_GBARUNNER2", 0);
	autorun = settingsini.GetInt("SRLOADER", "AUTORUNGAME", 0);
	showlogo = settingsini.GetInt("SRLOADER", "SHOWLOGO", 1);
	gotosettings = settingsini.GetInt("SRLOADER", "GOTOSETTINGS", 0);
	soundfreq = settingsini.GetInt("SRLOADER", "SOUND_FREQ", 0);
	flashcard = settingsini.GetInt("SRLOADER", "FLASHCARD", 0);
	resetSlot1 = settingsini.GetInt("SRLOADER", "RESET_SLOT1", 1);
	bootstrapFile = settingsini.GetInt("SRLOADER", "BOOTSTRAP_FILE", 0);
	launchType = settingsini.GetInt("SRLOADER", "LAUNCH_TYPE", 1);
	if (flashcardUsed && launchType == 0) launchType = 1;
	dsiWareSrlPath = settingsini.GetString("SRLOADER", "DSIWARE_SRL", "");
	dsiWarePubPath = settingsini.GetString("SRLOADER", "DSIWARE_PUB", "");
	dsiWarePrvPath = settingsini.GetString("SRLOADER", "DSIWARE_PRV", "");
	homebrewArg = settingsini.GetString("SRLOADER", "HOMEBREW_ARG", "");
	homebrewBootstrap = settingsini.GetInt("SRLOADER", "HOMEBREW_BOOTSTRAP", 0);
	consoleModel = settingsini.GetInt("SRLOADER", "CONSOLE_MODEL", 0);

	// Customizable UI settings.
	theme = settingsini.GetInt("SRLOADER", "THEME", 0);
	subtheme = settingsini.GetInt("SRLOADER", "SUB_THEME", 0);
	showDirectories = settingsini.GetInt("SRLOADER", "SHOW_DIRECTORIES", 1);
	showBoxArt = settingsini.GetInt("SRLOADER", "SHOW_BOX_ART", 1);
	animateDsiIcons = settingsini.GetInt("SRLOADER", "ANIMATE_DSI_ICONS", 0);

	// Default nds-bootstrap settings
	bstrap_language = settingsini.GetInt("NDS-BOOTSTRAP", "LANGUAGE", -1);
	boostCpu = settingsini.GetInt("NDS-BOOTSTRAP", "BOOST_CPU", 0);

	if(!flashcardUsed) {
		// nds-bootstrap
		CIniFile bootstrapini( bootstrapinipath );

		bstrap_debug = bootstrapini.GetInt("NDS-BOOTSTRAP", "DEBUG", 0);
		bstrap_romreadled = bootstrapini.GetInt("NDS-BOOTSTRAP", "ROMREAD_LED", 0);
		donorSdkVer = bootstrapini.GetInt( "NDS-BOOTSTRAP", "DONOR_SDK_VER", 0);
		bstrap_loadingScreen = bootstrapini.GetInt( "NDS-BOOTSTRAP", "LOADING_SCREEN", 1);
		// bstrap_lockARM9scfgext = bootstrapini.GetInt("NDS-BOOTSTRAP", "LOCK_ARM9_SCFG_EXT", 0);
	}
}

void SaveSettings(void) {
	// GUI
	CIniFile settingsini( settingsinipath );

	settingsini.SetInt("SRLOADER", "USE_GBARUNNER2", useGbarunner);
	settingsini.SetInt("SRLOADER", "AUTORUNGAME", autorun);
	settingsini.SetInt("SRLOADER", "SHOWLOGO", showlogo);
	settingsini.SetInt("SRLOADER", "GOTOSETTINGS", gotosettings);
	settingsini.SetInt("SRLOADER", "SOUND_FREQ", soundfreq);
	settingsini.SetInt("SRLOADER", "FLASHCARD", flashcard);
	settingsini.SetInt("SRLOADER", "RESET_SLOT1", resetSlot1);
	settingsini.SetInt("SRLOADER", "BOOTSTRAP_FILE", bootstrapFile);

	// UI settings.
	settingsini.SetInt("SRLOADER", "THEME", theme);
	settingsini.SetInt("SRLOADER", "SUB_THEME", subtheme);
	settingsini.SetInt("SRLOADER", "SHOW_DIRECTORIES", showDirectories);
	settingsini.SetInt("SRLOADER", "SHOW_BOX_ART", showBoxArt);
	settingsini.SetInt("SRLOADER", "ANIMATE_DSI_ICONS", animateDsiIcons);

	// Default nds-bootstrap settings
	settingsini.SetInt("NDS-BOOTSTRAP", "LANGUAGE", bstrap_language);
	settingsini.SetInt("NDS-BOOTSTRAP", "BOOST_CPU", boostCpu);
	settingsini.SaveIniFile(settingsinipath);

	if(!flashcardUsed) {
		// nds-bootstrap
		CIniFile bootstrapini( bootstrapinipath );

		bootstrapini.SetInt("NDS-BOOTSTRAP", "DEBUG", bstrap_debug);
		bootstrapini.SetInt("NDS-BOOTSTRAP", "ROMREAD_LED", bstrap_romreadled);
		bootstrapini.SetInt("NDS-BOOTSTRAP", "LOADING_SCREEN", bstrap_loadingScreen);
		// bootstrapini.SetInt("NDS-BOOTSTRAP", "LOCK_ARM9_SCFG_EXT", bstrap_lockARM9scfgext);
		bootstrapini.SaveIniFile(bootstrapinipath);
	}
}

int screenmode = 0;
int subscreenmode = 0;

static int settingscursor = 0;

static bool arm7SCFGLocked = false;

using namespace std;

mm_sound_effect snd_launch;
mm_sound_effect snd_select;
mm_sound_effect snd_stop;
mm_sound_effect snd_wrong;
mm_sound_effect snd_back;
mm_sound_effect snd_switch;

void InitSound() {
	mmInitDefaultMem((mm_addr)soundbank_bin);
	
	mmLoadEffect( SFX_LAUNCH );
	mmLoadEffect( SFX_SELECT );
	mmLoadEffect( SFX_STOP );
	mmLoadEffect( SFX_WRONG );
	mmLoadEffect( SFX_BACK );
	mmLoadEffect( SFX_SWITCH );

	snd_launch = {
		{ SFX_LAUNCH } ,			// id
		(int)(1.0f * (1<<10)),	// rate
		0,		// handle
		255,	// volume
		128,	// panning
	};
	snd_select = {
		{ SFX_SELECT } ,			// id
		(int)(1.0f * (1<<10)),	// rate
		0,		// handle
		255,	// volume
		128,	// panning
	};
	snd_stop = {
		{ SFX_STOP } ,			// id
		(int)(1.0f * (1<<10)),	// rate
		0,		// handle
		255,	// volume
		128,	// panning
	};
	snd_wrong = {
		{ SFX_WRONG } ,			// id
		(int)(1.0f * (1<<10)),	// rate
		0,		// handle
		255,	// volume
		128,	// panning
	};
	snd_back = {
		{ SFX_BACK } ,			// id
		(int)(1.0f * (1<<10)),	// rate
		0,		// handle
		255,	// volume
		128,	// panning
	};
	snd_switch = {
		{ SFX_SWITCH } ,			// id
		(int)(1.0f * (1<<10)),	// rate
		0,		// handle
		255,	// volume
		128,	// panning
	};
}

//---------------------------------------------------------------------------------
void stop (void) {
//---------------------------------------------------------------------------------
	while (1) {
		swiWaitForVBlank();
	}
}

char filePath[PATH_MAX];

//---------------------------------------------------------------------------------
void doPause(int x, int y) {
//---------------------------------------------------------------------------------
	// iprintf("Press start...\n");
	printSmall(false, x, y, "Press start...");
	while(1) {
		scanKeys();
		if(keysDown() & KEY_START)
			break;
	}
	scanKeys();
}

std::string ReplaceAll(std::string str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
    }
    return str;
}

void rebootDSiMenuPP() {
	fadeType = false;
	for (int i = 0; i < 25; i++) swiWaitForVBlank();
	memcpy((u32*)0x02000300,autoboot_bin,0x020);
	fifoSendValue32(FIFO_USER_08, 1);	// Reboot DSiMenu++ to avoid potential crashing
	for (int i = 0; i < 15; i++) swiWaitForVBlank();
}

void loadROMselect() {
	fadeType = false;
	for (int i = 0; i < 30; i++) swiWaitForVBlank();
	if (launchType == 2) {
		if (!access("sd:/bootthis.dsi", F_OK) && access(dsiWareSrlPath.c_str(), F_OK))
			rename ("sd:/bootthis.dsi", dsiWareSrlPath.c_str());	// Rename "bootthis.dsi" back to original .nds filename
		if (!access("sd:/bootthis.pub", F_OK) && access(dsiWarePubPath.c_str(), F_OK))
			rename ("sd:/bootthis.pub", dsiWarePubPath.c_str());
		if (!access("sd:/bootthis.prv", F_OK) && access(dsiWarePrvPath.c_str(), F_OK))
			rename ("sd:/bootthis.prv", dsiWarePrvPath.c_str());
	}
	renderScreens = false;
	if(soundfreq) fifoSendValue32(FIFO_USER_07, 2);
	else fifoSendValue32(FIFO_USER_07, 1);
	if (theme==2) {
		runNdsFile ("/_nds/dsimenuplusplus/r4menu.srldr", 0, NULL, false);
	} else {
		runNdsFile ("/_nds/dsimenuplusplus/dsimenu.srldr", 0, NULL, false);
	}
}

int lastRanROM() {
	fadeType = false;
	for (int i = 0; i < 30; i++) swiWaitForVBlank();
	renderScreens = false;
	if(soundfreq) fifoSendValue32(FIFO_USER_07, 2);
	else fifoSendValue32(FIFO_USER_07, 1);

	vector<char*> argarray;
	if (launchType > 2) {
		argarray.push_back(strdup("null"));
		argarray.push_back(strdup(homebrewArg.c_str()));
	}

	int err = 0;
	if (launchType == 0) {
		err = runNdsFile ("/_nds/dsimenuplusplus/slot1launch.srldr", 0, NULL, true);
	} else if (launchType == 1) {
		if (!flashcardUsed) {
			if (homebrewBootstrap) {
				bootstrapfilename = "sd:/_nds/hb-bootstrap.nds";
			} else {
				if (donorSdkVer==5) {
					if (bootstrapFile) bootstrapfilename = "sd:/_nds/nightly-bootstrap-sdk5.nds";
					else bootstrapfilename = "sd:/_nds/release-bootstrap-sdk5.nds";
				} else {
					if (bootstrapFile) bootstrapfilename = "sd:/_nds/nightly-bootstrap.nds";
					else bootstrapfilename = "sd:/_nds/release-bootstrap.nds";
				}
			}
			err = runNdsFile (bootstrapfilename.c_str(), 0, NULL, true);
		} else {
			switch (flashcard) {
				case 0:
				case 1:
				default:
					err = runNdsFile ("fat:/YSMenu.nds", 0, NULL, true);
					break;
				case 2:
				case 4:
				case 5:
					err = runNdsFile ("fat:/Wfwd.dat", 0, NULL, true);
					break;
				case 3:
					err = runNdsFile ("fat:/Afwd.dat", 0, NULL, true);
					break;
				case 6:
					err = runNdsFile ("fat:/_dstwo/autoboot.nds", 0, NULL, true);
					break;
			}
		}
	} else if (launchType == 2) {
		if (!access(dsiWareSrlPath.c_str(), F_OK) && access("sd:/bootthis.dsi", F_OK))
			rename (dsiWareSrlPath.c_str(), "sd:/bootthis.dsi");	// Rename .nds file to "bootthis.dsi" for Unlaunch to boot it
		if (!access(dsiWarePubPath.c_str(), F_OK) && access("sd:/bootthis.pub", F_OK))
			rename (dsiWarePubPath.c_str(), "sd:/bootthis.pub");
		if (!access(dsiWarePrvPath.c_str(), F_OK) && access("sd:/bootthis.prv", F_OK))
			rename (dsiWarePrvPath.c_str(), "sd:/bootthis.prv");

		fifoSendValue32(FIFO_USER_08, 1);	// Reboot
	} else if (launchType == 3) {
		if(flashcardUsed) {
			argarray.at(0) = "/_nds/dsimenuplusplus/emulators/nesds.nds";
			err = runNdsFile ("/_nds/dsimenuplusplus/emulators/nesds.nds", argarray.size(), (const char **)&argarray[0], true);	// Pass ROM to nesDS as argument
		} else {
			argarray.at(0) = "sd:/_nds/dsimenuplusplus/emulators/nestwl.nds";
			err = runNdsFile ("sd:/_nds/dsimenuplusplus/emulators/nestwl.nds", argarray.size(), (const char **)&argarray[0], true);	// Pass ROM to nesDS as argument
		}
	} else if (launchType == 4) {
		if(flashcardUsed) {
			argarray.at(0) = "/_nds/dsimenuplusplus/emulators/gameyob.nds";
			err = runNdsFile ("/_nds/dsimenuplusplus/emulators/gameyob.nds", argarray.size(), (const char **)&argarray[0], true);	// Pass ROM to GameYob as argument
		} else {
			argarray.at(0) = "sd:/_nds/dsimenuplusplus/emulators/gameyob.nds";
			err = runNdsFile ("sd:/_nds/dsimenuplusplus/emulators/gameyob.nds", argarray.size(), (const char **)&argarray[0], true);	// Pass ROM to GameYob as argument
		}
	}
	
	return err;
}

//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
//---------------------------------------------------------------------------------

	// Turn on screen backlights if they're disabled
	powerOn(PM_BACKLIGHT_TOP);
	powerOn(PM_BACKLIGHT_BOTTOM);

	// overwrite reboot stub identifier
	extern u64 *fake_heap_end;
	*fake_heap_end = 0;

	defaultExceptionHandler();

	// Read user name
	char *username = (char*)PersonalData->name;

	// text
	for (int i = 0; i < 10; i++) {
		if (username[i*2] == 0x00)
			username[i*2/2] = 0;
		else
			username[i*2/2] = username[i*2];
	}

	if (!fatInitDefault()) {
		graphicsInit();
		fontInit();
		fadeType = true;
		printf("\n ");
		printf(username);
		printSmall(false, 4, 4, "fatinitDefault failed!");
		stop();
	}

	if (!access("fat:/", F_OK)) flashcardUsed = true;

	bool soundfreqsetting = false;

	std::string filename;
	
	LoadSettings();
	
	swiWaitForVBlank();

	fifoWaitValue32(FIFO_USER_06);
	if (fifoGetValue32(FIFO_USER_03) == 0) arm7SCFGLocked = true;	// If DSiMenu++ is being ran from DSiWarehax or flashcard, then arm7 SCFG is locked.

	u16 arm7_SNDEXCNT = fifoGetValue32(FIFO_USER_07);
	if (arm7_SNDEXCNT != 0) soundfreqsetting = true;
	fifoSendValue32(FIFO_USER_07, 0);

	scanKeys();

	if (arm7SCFGLocked && !gotosettings && autorun && !(keysHeld() & KEY_B)) {
		lastRanROM();
	}
	
	InitSound();
	
	char vertext[12];
	// snprintf(vertext, sizeof(vertext), "Ver %d.%d.%d   ", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH); // Doesn't work :(
	snprintf(vertext, sizeof(vertext), "Ver %d.%d.%d   ", 5, 1, 0);

	if (gotosettings) {
		graphicsInit();
		fontInit();
		screenmode = 1;
		gotosettings = false;
		SaveSettings();
		fadeType = true;
	} else if (autorun || showlogo) {
		loadTitleGraphics();
		fadeType = true;

		for (int i = 0; i < 60*3; i++) {
			swiWaitForVBlank();
		}
		
		scanKeys();

		if (keysHeld() & KEY_START) {
			fadeType = false;
			for (int i = 0; i < 30; i++) {
				swiWaitForVBlank();
			}
			graphicsInit();
			fontInit();
			screenmode = 1;
			fadeType = true;
		}
	} else {
		scanKeys();

		if (keysHeld() & KEY_START) {
			graphicsInit();
			fontInit();
			fadeType = true;
			screenmode = 1;
			for (int i = 0; i < 60; i++) {
				swiWaitForVBlank();
			}
		}
	}


	srand(time(NULL));

	bool menuprinted = false;
	//const char* lrswitchpages = "L/R: Switch pages";

	bool hiyaAutobootFound = false;

	int pressed = 0;

	while(1) {
	
		if (screenmode == 1) {

			consoleClear();
			printf("\n ");
			printf(username);
			for (int i = 0; i < 21; i++) {
				printf("\n");
			}

			if (subscreenmode == 3) {
				pressed = 0;

				if (!menuprinted) {
					printSmall(true, 194, 176, vertext);

					// Clear the screen so it doesn't over-print
					clearText();

					printLarge(false, 6, 4, "Flashcard(s) select");

					int yPos = 32;
					switch (flashcard) {
						case 0:
						default:
							printSmall(false, 12, 32+(0*14), "DSTT");
							printSmall(false, 12, 32+(1*14), "R4i Gold");
							printSmall(false, 12, 32+(2*14), "R4i-SDHC (Non-v1.4.x version) (www.r4i-sdhc.com)");
							printSmall(false, 12, 32+(3*14), "R4 SDHC Dual-Core");
							printSmall(false, 12, 32+(4*14), "R4 SDHC Upgrade");
							printSmall(false, 12, 32+(5*14), "SuperCard DSONE");
							break;
						case 1:
							printSmall(false, 12, 32+(0*14), "Original R4");
							printSmall(false, 12, 32+(1*14), "M3 Simply");
							break;
						case 2:
							printSmall(false, 12, 32+(0*14), "R4iDSN");
							printSmall(false, 12, 32+(1*14), "R4i Gold RTS");
							printSmall(false, 12, 32+(2*14), "R4 Ultra");
							break;
						case 3:
							printSmall(false, 12, 32+(0*14), "Acekard 2(i)");
							printSmall(false, 12, 32+(1*14), "Galaxy Eagle");
							printSmall(false, 12, 32+(2*14), "M3DS Real");
							break;
						case 4:
							printSmall(false, 12, 32, "Acekard RPG");
							break;
						case 5:
							printSmall(false, 12, 32+(0*14), "Ace 3DS+");
							printSmall(false, 12, 32+(1*14), "Gateway Blue Card");
							printSmall(false, 12, 32+(2*14), "R4iTT");
							break;
						case 6:
							printSmall(false, 12, 32, "SuperCard DSTWO");
							break;
					}

					printLargeCentered(true, 120, "Left/Right: Select flashcard(s)");
					printLargeCentered(true, 134, "A/B: Set and return");

					menuprinted = true;
				}

				// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
				do {
					scanKeys();
					pressed = keysDownRepeat();
					swiWaitForVBlank();
				} while (!pressed);
				
				if (pressed & KEY_LEFT) {
					flashcard -= 1;
					mmEffectEx(&snd_select);
					menuprinted = false;
				}
				if (pressed & KEY_RIGHT) {
					flashcard += 1;
					mmEffectEx(&snd_select);
					menuprinted = false;
				}

				if ((pressed & KEY_A) || (pressed & KEY_B)) {
					subscreenmode = 1;
					mmEffectEx(&snd_select);
					menuprinted = false;
				}

				if (flashcard > 6) flashcard = 0;
				else if (flashcard < 0) flashcard = 6;
			} else if (subscreenmode == 2) {
				pressed = 0;

				if (!menuprinted) {
					printSmall(true, 194, 176, vertext);

					// Clear the screen so it doesn't over-print
					clearText();

					switch (theme) {
						case 0:
						default:
							printLarge(false, 6, 4, "Sub-theme select: DSi Menu");
							break;
						case 1:
							printLarge(false, 6, 4, "Sub-theme select: 3DS HOME Menu");
							break;
						case 2:
							printLarge(false, 6, 4, "Sub-theme select: R4");
							break;
					}

					int yPos = 32;
					if (theme == 2) yPos = 24;
					for (int i = 0; i < subtheme; i++) {
						yPos += 12;
					}

					printSmall(false, 4, yPos, ">");

					int selyPos = 32;
					if (theme == 2) selyPos = 24;

					switch (theme) {
						case 0:
						default:
							printSmall(false, 12, 32, "SD Card Menu");
							selyPos += 12;
							printSmall(false, 12, 46, "Normal Menu");
							break;
						case 1:
							//printSmall(false, 12, 32, "DS Menu");
							//selyPos += 12;
							//printSmall(false, 12, 46, "3DS HOME Menu");
							break;
						case 2:
							printSmall(false, 12, selyPos, "01: Snow hill");
							selyPos += 12;
							printSmall(false, 12, selyPos, "02: Snow land");
							selyPos += 12;
							printSmall(false, 12, selyPos, "03: Green leaf");
							selyPos += 12;
							printSmall(false, 12, selyPos, "04: Pink flower");
							selyPos += 12;
							printSmall(false, 12, selyPos, "05: Park");
							selyPos += 12;
							printSmall(false, 12, selyPos, "06: Cherry blossoms");
							selyPos += 12;
							printSmall(false, 12, selyPos, "07: Beach");
							selyPos += 12;
							printSmall(false, 12, selyPos, "08: Summer sky");
							selyPos += 12;
							printSmall(false, 12, selyPos, "09: River");
							selyPos += 12;
							printSmall(false, 12, selyPos, "10: Fall trees");
							selyPos += 12;
							printSmall(false, 12, selyPos, "11: Christmas tree");
							selyPos += 12;
							printSmall(false, 12, selyPos, "12: Drawn symbol");
							break;
					}

					printLargeCentered(true, 128, "A/B: Set sub-theme.");

					menuprinted = true;
				}

				// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
				do {
					scanKeys();
					pressed = keysDownRepeat();
					swiWaitForVBlank();
				} while (!pressed);
				
				if (pressed & KEY_UP) {
					subtheme -= 1;
					mmEffectEx(&snd_select);
					menuprinted = false;
				}
				if (pressed & KEY_DOWN) {
					subtheme += 1;
					mmEffectEx(&snd_select);
					menuprinted = false;
				}

				if ((pressed & KEY_A) || (pressed & KEY_B)) {
					subscreenmode = 0;
					mmEffectEx(&snd_select);
					menuprinted = false;
				}

				if (theme == 2) {
					if (subtheme > 11) subtheme = 0;
					else if (subtheme < 0) subtheme = 11;
				} else {
					if (subtheme > 1) subtheme = 0;
					else if (subtheme < 0) subtheme = 1;
				}
			} else if (subscreenmode == 1) {
				pressed = 0;

				if (!menuprinted) {
					// Clear the screen so it doesn't over-print
					clearText();

					printSmallCentered(false, 175, "DSiMenu++");

					printSmall(true, 4, 176, "^/~ Switch pages");
					printSmall(true, 194, 176, vertext);

					printLarge(false, 6, 4, "Games and Apps settings");

					int yPos = 32;
					for (int i = 0; i < settingscursor; i++) {
						yPos += 12;
					}

					int selyPos = 32;

					printSmall(false, 4, yPos, ">");

					if(!flashcardUsed) {
						printSmall(false, 12, selyPos, "Language");
						switch(bstrap_language) {
							case -1:
							default:
								printSmall(false, 203, selyPos, "System");
								break;
							case 0:
								printSmall(false, 194, selyPos, "Japanese");
								break;
							case 1:
								printSmall(false, 206, selyPos, "English");
								break;
							case 2:
								printSmall(false, 207, selyPos, "French");
								break;
							case 3:
								printSmall(false, 200, selyPos, "German");
								break;
							case 4:
								printSmall(false, 210, selyPos, "Italian");
								break;
							case 5:
								printSmall(false, 203, selyPos, "Spanish");
								break;
						}
						selyPos += 12;

						printSmall(false, 12, selyPos, "ARM9 CPU Speed");
						if(boostCpu)
							printSmall(false, 158, selyPos, "133mhz (TWL)");
						else
							printSmall(false, 170, selyPos, "67mhz (NTR)");
						selyPos += 12;

						printSmall(false, 12, selyPos, "Debug");
						if(bstrap_debug)
							printSmall(false, 224, selyPos, "On");
						else
							printSmall(false, 224, selyPos, "Off");
						selyPos += 12;
						if (consoleModel < 2) {
							printSmall(false, 12, selyPos, "ROM read LED");
							switch(bstrap_romreadled) {
								case 0:
								default:
									romreadled_valuetext = "None";
									break;
								case 1:
									romreadled_valuetext = "WiFi";
									break;
								case 2:
									romreadled_valuetext = "Power";
									break;
								case 3:
									romreadled_valuetext = "Camera";
									break;
							}
							printSmall(false, 216, selyPos, romreadled_valuetext);
						}
						selyPos += 12;

						printSmall(false, 12, selyPos, "Sound/Mic frequency");
						if(soundfreq)
							printSmall(false, 187, selyPos, "47.61 kHz");
						else
							printSmall(false, 186, selyPos, "32.73 kHz");
						selyPos += 12;

						if (!arm7SCFGLocked) {
							printSmall(false, 12, selyPos, "Reset Slot-1");
							if(resetSlot1)
								printSmall(false, 224, selyPos, "Yes");
							else
								printSmall(false, 230, selyPos, "No");
						}
						selyPos += 12;

						printSmall(false, 12, selyPos, "Loading screen");
						switch(bstrap_loadingScreen) {
							case 0:
							default:
								printSmall(false, 216, selyPos, "None");
								break;
							case 1:
								printSmall(false, 200, selyPos, "Regular");
								break;
							case 2:
								printSmall(false, 216, selyPos, "Pong");
								break;
							case 3:
								printSmall(false, 172, selyPos, "Tic-Tac-Toe");
								break;
						}
						selyPos += 12;

						printSmall(false, 12, selyPos, "Bootstrap");
						if(bootstrapFile)
							printSmall(false, 202, selyPos, "Nightly");
						else
							printSmall(false, 200, selyPos, "Release");


						if (settingscursor == 0) {
							printLargeCentered(true, 114, "Avoid the limited selections");
							printLargeCentered(true, 128, "of your console language");
							printLargeCentered(true, 142, "by setting this option.");
						} else if (settingscursor == 1) {
							printLargeCentered(true, 120, "Set to TWL to get rid of lags");
							printLargeCentered(true, 134, "in some games.");
						} /* else if (settingscursor == 4) {
							printLargeCentered(true, 120, "Allows 8 bit VRAM writes");
							printLargeCentered(true, 134, "and expands the bus to 32 bit.");
						} */ else if (settingscursor == 2) {
							printLargeCentered(true, 120, "Displays some text before");
							printLargeCentered(true, 134, "launched game.");
						} else if (settingscursor == 3) {
							// printLargeCentered(true, 114, "Locks the ARM9 SCFG_EXT,");
							// printLargeCentered(true, 128, "avoiding conflict with");
							// printLargeCentered(true, 142, "recent libnds.");
							printLargeCentered(true, 128, "Sets LED as ROM read indicator.");
						} else if (settingscursor == 4) {
							printLargeCentered(true, 120, "32.73 kHz: Original quality");
							printLargeCentered(true, 134, "47.61 kHz: High quality");
						} else if (settingscursor == 5) {
							printLargeCentered(true, 120, "Enable this if Slot-1 cards are");
							printLargeCentered(true, 134, "stuck on white screens.");
						} else if (settingscursor == 6) {
							printLargeCentered(true, 120, "Shows a loading screen before ROM");
							printLargeCentered(true, 134, "is started in nds-bootstrap.");
						} else if (settingscursor == 7) {
							printLargeCentered(true, 120, "Pick release or nightly");
							printLargeCentered(true, 134, "bootstrap.");
						}
					} else {
						printSmall(false, 12, selyPos, "Flashcard(s) select");
						selyPos += 12;
						if(soundfreqsetting) {
							printSmall(false, 12, selyPos, "Sound/Mic frequency");
							if(soundfreq)
								printSmall(false, 184, selyPos, "47.61 kHz");
							else
								printSmall(false, 184, selyPos, "32.73 kHz");
						} else {
							printSmall(false, 12, selyPos, "Use GBARunner2");
							if(useGbarunner)
								printSmall(false, 224, selyPos, "Yes");
							else
								printSmall(false, 224, selyPos, "No");
						}

						if (settingscursor == 0) {
							printLargeCentered(true, 120, "Pick a flashcard to use to");
							printLargeCentered(true, 134, "run ROMs from it.");
						} else if (settingscursor == 1) {
							if(soundfreqsetting) {
								printLargeCentered(true, 120, "32.73 kHz: Original quality");
								printLargeCentered(true, 134, "47.61 kHz: High quality");
							} else {
								printLargeCentered(true, 120, "Use either GBARunner2 or the");
								printLargeCentered(true, 134, "native GBA mode to play GBA games.");
							}
						}
					}



					menuprinted = true;
				}

				// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
				do {
					scanKeys();
					pressed = keysDownRepeat();
					swiWaitForVBlank();
				} while (!pressed);
				
				if (pressed & KEY_UP) {
					settingscursor--;
					if (consoleModel > 1 && settingscursor == 3) settingscursor--;
					if (arm7SCFGLocked && settingscursor == 5) settingscursor--;
					mmEffectEx(&snd_select);
					menuprinted = false;
				}
				if (pressed & KEY_DOWN) {
					settingscursor++;
					if (consoleModel > 1 && settingscursor == 3) settingscursor++;
					if (arm7SCFGLocked && settingscursor == 5) settingscursor++;
					mmEffectEx(&snd_select);
					menuprinted = false;
				}
					
				if ((pressed & KEY_A) || (pressed & KEY_LEFT) || (pressed & KEY_RIGHT)) {
					if(!flashcardUsed) {
						switch (settingscursor) {
							case 0:
							default:
								if (pressed & KEY_LEFT) {
									bstrap_language--;
									if (bstrap_language < -1) bstrap_language = 5;
								} else if ((pressed & KEY_RIGHT) || (pressed & KEY_A)) {
									bstrap_language++;
									if (bstrap_language > 5) bstrap_language = -1;
								}
								break;
							case 1:
								boostCpu = !boostCpu;
								break;
							case 2:
								bstrap_debug = !bstrap_debug;
								break;
							case 3:
								// bstrap_lockARM9scfgext = !bstrap_lockARM9scfgext;
								if (pressed & KEY_LEFT) {
									bstrap_romreadled--;
									if (bstrap_romreadled < 0) bstrap_romreadled = 2;
								} else if ((pressed & KEY_RIGHT) || (pressed & KEY_A)) {
									bstrap_romreadled++;
									if (bstrap_romreadled > 2) bstrap_romreadled = 0;
								}
								break;
							case 4:
								soundfreq = !soundfreq;
								break;
							case 5:
								resetSlot1 = !resetSlot1;
								break;
							case 6:
								if (pressed & KEY_LEFT) {
									bstrap_loadingScreen--;
									if (bstrap_loadingScreen < 0) bstrap_loadingScreen = 3;
								} else if ((pressed & KEY_RIGHT) || (pressed & KEY_A)) {
									bstrap_loadingScreen++;
									if (bstrap_loadingScreen > 3) bstrap_loadingScreen = 0;
								}
								break;
							case 7:
								bootstrapFile = !bootstrapFile;
								break;
						}
					} else {
						switch (settingscursor) {
							case 0:
							default:
								subscreenmode = 3;
								break;
							case 1:
								if(soundfreqsetting) soundfreq = !soundfreq;
								else useGbarunner = !useGbarunner;
								break;
						}
					}
					mmEffectEx(&snd_select);
					menuprinted = false;
				}
				
				if ((pressed & KEY_L) || (pressed & KEY_R)) {
					subscreenmode = 0;
					settingscursor = 0;
					mmEffectEx(&snd_switch);
					menuprinted = false;
				}

				if (pressed & KEY_B) {
					mmEffectEx(&snd_back);
					clearText();
					printSmall(false, 4, 4, "Saving settings...");
					SaveSettings();
					clearText();
					printSmall(false, 4, 4, "Settings saved!");
					for (int i = 0; i < 60; i++) swiWaitForVBlank();
					if (!arm7SCFGLocked) {
						rebootDSiMenuPP();
					}
					loadROMselect();
					break;
				}

				if(!flashcardUsed) {
					if (settingscursor > 7) settingscursor = 0;
					else if (settingscursor < 0) settingscursor = 7;
				} else {
					if (settingscursor > 1) settingscursor = 0;
					else if (settingscursor < 0) settingscursor = 1;
				}
			} else {
				pressed = 0;

				if (!flashcardUsed && consoleModel < 2) {
					if (!access("sd:/hiya/autoboot.bin", F_OK)) hiyaAutobootFound = true;
					else hiyaAutobootFound = false;
				}

				if (!menuprinted) {
					// Clear the screen so it doesn't over-print
					clearText();

					printSmallCentered(false, 175, "DSiMenu++");

					printSmall(true, 4, 176, "^/~ Switch pages");
					printSmall(true, 194, 176, vertext);

					printLarge(false, 6, 4, "GUI settings");
					
					int yPos = 32;
					for (int i = 0; i < settingscursor; i++) {
						yPos += 12;
					}

					int selyPos = 32;

					printSmall(false, 4, yPos, ">");

					printSmall(false, 12, selyPos, "Theme");
					switch (theme) {
						case 0:
						default:
							printSmall(false, 224, selyPos, "DSi");
							break;
						case 1:
							printSmall(false, 224, selyPos, "3DS");
							break;
						case 2:
							printSmall(false, 224, selyPos, "R4");
							break;
					}
					selyPos += 12;

					printSmall(false, 12, selyPos, "Last played ROM on startup");
					if(autorun)
						printSmall(false, 224, selyPos, "Yes");
					else
						printSmall(false, 230, selyPos, "No");
					selyPos += 12;

					printSmall(false, 12, selyPos, "DSiMenu++ logo on startup");
					if(showlogo)
						printSmall(false, 216, selyPos, "Show");
					else
						printSmall(false, 222, selyPos, "Hide");
					selyPos += 12;

					printSmall(false, 12, selyPos, "Directories/folders");
					if(showDirectories)
						printSmall(false, 216, selyPos, "Show");
					else
						printSmall(false, 222, selyPos, "Hide");
					selyPos += 12;

					printSmall(false, 12, selyPos, "Box art/Game covers");
					if(showBoxArt)
						printSmall(false, 216, selyPos, "Show");
					else
						printSmall(false, 222, selyPos, "Hide");
					selyPos += 12;

					printSmall(false, 12, selyPos, "Animate DSi icons");
					if(animateDsiIcons)
						printSmall(false, 224, selyPos, "Yes");
					else
						printSmall(false, 230, selyPos, "No");
					selyPos += 12;

					if (!flashcardUsed && !arm7SCFGLocked) {
						if (consoleModel < 2) {
							if (hiyaAutobootFound) {
								printSmall(false, 12, selyPos, "Restore DSi Menu");
							} else {
								printSmall(false, 12, selyPos, "Replace DSi Menu");
							}
						}
					}


					if (settingscursor == 0) {
						printLargeCentered(true, 120, "The theme to use in DSiMenu++.");
						printLargeCentered(true, 134, "Press A for sub-themes.");
					} else if (settingscursor == 1) {
						printLargeCentered(true, 106, "If turned on, hold B on");
						printLargeCentered(true, 120, "startup to skip to the");
						printLargeCentered(true, 134, "ROM select menu.");
						printLargeCentered(true, 148, "Press Y to start last played ROM.");
					} else if (settingscursor == 2) {
						printLargeCentered(true, 114, "The DSiMenu++ logo will be");
						printLargeCentered(true, 128, "shown when you start");
						printLargeCentered(true, 142, "DSiMenu++.");
					} else if (settingscursor == 3) {
						printLargeCentered(true, 114, "If you're in a folder where most");
						printLargeCentered(true, 128, "of your games are, it is safe to");
						printLargeCentered(true, 142, "hide directories/folders.");
					} else if (settingscursor == 4) {
						printLargeCentered(true, 120, "Displayed in the top screen");
						printLargeCentered(true, 134, "of the DSi/3DS theme.");
					} else if (settingscursor == 5) {
						printLargeCentered(true, 114, "Animates DSi-enhanced icons like in");
						printLargeCentered(true, 128, "the DSi/3DS menus. Turning this off");
						printLargeCentered(true, 142, "will fix some icons appearing white.");
					} else if (settingscursor == 6) {
						if (hiyaAutobootFound) {
							printLargeCentered(true, 128, "Show DSi Menu on boot again.");
						} else {
							printLargeCentered(true, 120, "Start DSiMenu++ on boot, instead.");
							printLargeCentered(true, 134, "of the regular DSi Menu.");
						}
					}


					menuprinted = true;
				}

				// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
				do {
					scanKeys();
					pressed = keysDownRepeat();
					swiWaitForVBlank();
				} while (!pressed);

				if (pressed & KEY_UP) {
					settingscursor -= 1;
					mmEffectEx(&snd_select);
					menuprinted = false;
				}
				if (pressed & KEY_DOWN) {
					settingscursor += 1;
					mmEffectEx(&snd_select);
					menuprinted = false;
				}

				if ((pressed & KEY_A) || (pressed & KEY_LEFT) || (pressed & KEY_RIGHT)) {
					switch (settingscursor) {
						case 0:
						default:
							if (pressed & KEY_LEFT) {
								subtheme = 0;
								theme -= 1;
								if (theme < 0) theme = 2;
								mmEffectEx(&snd_select);
							} else if (pressed & KEY_RIGHT) {
								subtheme = 0;
								theme += 1;
								if (theme > 2) theme = 0;
								mmEffectEx(&snd_select);
							} else if (theme == 1) {
								mmEffectEx(&snd_wrong);
							} else {
								subscreenmode = 2;
								mmEffectEx(&snd_select);
							}
							break;
						case 1:
							autorun = !autorun;
							mmEffectEx(&snd_select);
							break;
						case 2:
							showlogo = !showlogo;
							mmEffectEx(&snd_select);
							break;
						case 3:
							showDirectories = !showDirectories;
							mmEffectEx(&snd_select);
							break;
						case 4:
							showBoxArt = !showBoxArt;
							mmEffectEx(&snd_select);
							break;
						case 5:
							animateDsiIcons = !animateDsiIcons;
							mmEffectEx(&snd_select);
							break;
						case 6:
							if (pressed & KEY_A) {
								if (hiyaAutobootFound) {
									if ( remove ("sd:/hiya/autoboot.bin") != 0 ) {
									} else {
										hiyaAutobootFound = false;
									}
								} else {
									FILE* ResetData = fopen("sd:/hiya/autoboot.bin","wb");
									fwrite(autoboot_bin,1,autoboot_bin_len,ResetData);
									fclose(ResetData);
									hiyaAutobootFound = true;

									CIniFile hiyacfwini( hiyacfwinipath );
									hiyacfwini.SetInt("HIYA-CFW", "TITLE_AUTOBOOT", 1);
									hiyacfwini.SaveIniFile(hiyacfwinipath);
								}
							}
							break;
					}
					menuprinted = false;
				}

				if ((pressed & KEY_L) || (pressed & KEY_R)) {
					subscreenmode = 1;
					settingscursor = 0;
					mmEffectEx(&snd_switch);
					menuprinted = false;
				}

				if (pressed & KEY_Y && settingscursor == 1) {
					screenmode = 0;
					mmEffectEx(&snd_launch);
					clearText();
					printSmall(false, 4, 4, "Saving settings...");
					SaveSettings();
					clearText();
					printSmall(false, 4, 4, "Settings saved!");
					for (int i = 0; i < 60; i++) swiWaitForVBlank();
					int err = lastRanROM();
					iprintf ("Start failed. Error %i\n", err);
				}

				if (pressed & KEY_B) {
					mmEffectEx(&snd_back);
					clearText();
					printSmall(false, 4, 4, "Saving settings...");
					SaveSettings();
					clearText();
					printSmall(false, 4, 4, "Settings saved!");
					for (int i = 0; i < 60; i++) swiWaitForVBlank();
					if (!arm7SCFGLocked) {
						rebootDSiMenuPP();
					}
					loadROMselect();
					break;
				}

				if (!flashcardUsed) {
					if (consoleModel < 2) {
						if (settingscursor > 6) settingscursor = 0;
						else if (settingscursor < 0) settingscursor = 6;
					} else {
						if (settingscursor > 5) settingscursor = 0;
						else if (settingscursor < 0) settingscursor = 5;
					}
				} else {
					if (settingscursor > 5) settingscursor = 0;
					else if (settingscursor < 0) settingscursor = 5;
				}
			}

		} else {
			loadROMselect();
		}

	}

	return 0;
}
