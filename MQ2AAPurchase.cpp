// MQ2AAPurchase.cpp : Defines the entry point for the DLL application.
//

// MQ2AAPurchase by mackal
// Mostly based on RedGuides help page for their MQ2AASpend
// GPL


#include "../MQ2Plugin.h"
#include "MQ2AAPurchase.h"
#include "../Blech/Blech.h"
#include <cstring>

PreSetup("MQ2AAPurchase");

// Called once, when the plugin is to initialize
PLUGIN_API VOID InitializePlugin(VOID)
{
	DebugSpewAlways("Initializing MQ2AAPurchase");
	pAAEvents = new Blech('#');

	pAAEvents->AddEvent("You have gained an ability point!  You now have #pnts# ability point.", GainedAa);
	pAAEvents->AddEvent("You have gained an ability point!  You now have #pnts# ability points.", GainedAa);
	pAAEvents->AddEvent("You have gained #*# ability point(s)!  You now have #pnts# ability point(s).", GainedAa);
	pAAEvents->AddEvent("You have gained a level! Welcome to level #*#!", GainedLevel);
	pAAEvents->AddEvent("You have gained #*# levels! Welcome to level #*#!", GainedLevel);

	AddCommand("/aapurchase", cmdAapurchase);
}

// Called once, when the plugin is to shutdown
PLUGIN_API VOID ShutdownPlugin(VOID)
{
	DebugSpewAlways("Shutting down MQ2AAPurchase");
	delete pAAEvents;
	pAAEvents = NULL;

	RemoveCommand("/aapurchase");
}

// Called once directly after initialization, and then every time the gamestate changes
PLUGIN_API VOID SetGameState(DWORD GameState)
{
	if (GameState == GAMESTATE_INGAME) {
		if (!g_aainit) {
			snprintf(INIFileName, sizeof(INIFileName), "%s\\%s_%s.ini", gszINIPath, EQADDR_SERVERNAME, GetCharInfo()->Name);
			LoadINI();
			g_aainit = true;
		}
	} else if (GameState != GAMESTATE_LOGGINGIN) {
		if (g_aainit)
			g_aainit = false;
	}
}


// This is called every time MQ pulses
PLUGIN_API VOID OnPulse(VOID)
{
	if (GetGameState() != GAMESTATE_INGAME)
		return;

	// Time to buy some AAs
	if (g_autopurchase) {
		if (g_frame_count != SKIPPED_FRAMES) { // rate limit
			g_frame_count++;
			return;
		}

		// end conditions: aa_list is empty, we're at the end of the list, we have no more AA Points
		if (aa_list.empty() || g_position == aa_list.size() || !GetCharInfo2()->AAPoints) {
			g_frame_count = g_position = 0;
			g_autopurchase = false;
			return;
		}

		char szTemp[MAX_STRING] = { 0 };
		PALTABILITY aa = NULL;

		if (aa_list[g_position].id != -1 && (aa = pAltAdvManager->GetAAById(aa_list[g_position].id))) {
			// make sure we are set to buy this rank or max out the AA. CanTrainAbility does the rest of the work (cost, level, etc)
			if ((aa->CurrentRank <= aa_list[g_position].rank || aa_list[g_position].max) && pAltAdvManager->CanTrainAbility((PcZoneClient *)pCharData, (CAltAbilityData *)aa, 0, 0, 0)) {
				snprintf(szTemp, sizeof(szTemp), "/alt buy %d", aa->Index);
				EzCommand(szTemp);
				aa_list[g_position].id = aa->NextGroupAbilityId;
				aa_list[g_position].current_rank = aa->CurrentRank;
			} else {
				g_position++;
			}
		} else {
			g_position++;
		}
		g_frame_count = 0;
	}
}

PLUGIN_API DWORD OnIncomingChat(PCHAR Line, DWORD Color)
{
	if (pAAEvents && Color == 15) {// Limit to the colors we know the messages are
		char szLine[MAX_STRING] = { 0 };
		strcpy_s(szLine, Line);
		pAAEvents->Feed(szLine);
	}
	return 0;
}

// It's possible for the AA manager to know more than 1 AA by that name, so we need to make sure we know this might not be the AA we want
PALTABILITY GetAAFromName(const char *name)
{
	if (!name || !name[0])
		return NULL;

	for (unsigned long nAbility = 0; nAbility < NUM_ALT_ABILITIES; nAbility++)
		if (PALTABILITY pAbility = pAltAdvManager->GetAAById(nAbility))
			if (PCHAR pName = pCDBStr->GetString(pAbility->nName, 1, NULL))
				if (!_stricmp(name, pName))
					return pAbility;

	return NULL;
}

void LoadINI()
{
	char szTemp[MAX_STRING] = { 0 }, szKeys[MAX_STRING] = { 0 }, *token = NULL, *key = NULL, *next = NULL;
	AAEntry temp;
	aa_list.clear();
	g_banked = GetPrivateProfileInt("MQ2AAPurchase_Settings", "BankPoints", 40, INIFileName);
	g_auton = GetPrivateProfileInt("MQ2AAPurchase_Settings", "AutoSpend", 1, INIFileName) != 0 ? true : false;
	// Get the list of key names
	if (!GetPrivateProfileString("MQ2AAPurchase_List", NULL, NULL, szKeys, sizeof(szKeys), INIFileName))
		return; // nothing loaded!
	key = szKeys; // set to our key
	while (true) {
		temp.clear();
		szTemp[0] = '\0';

		if (!GetPrivateProfileString("MQ2AAPurchase_List", key, NULL, szTemp, MAX_STRING, INIFileName))
			break; // didn't load an INI entry, must be done!

		if (!(token = strtok_s(szTemp, "|", &next))) // Gotos are fine for errors!
			goto NextKey; // not a well formed INI entry, move on
		temp.name = token;

		if (!(token = strtok_s(NULL, "|", &next)))
			goto NextKey; // not a well formed INI entry, move on
		if (token[0] == 'M' || token[0] == 'm')
			temp.max = true;
		else // must be a rank
			temp.rank = atoi(token);

		PALTABILITY aa = GetAAFromName(temp.name.c_str());
		if (aa) {
			if (PALTABILITY aatemp = GetMaxOwned(aa)) {
				temp.id = aatemp->NextGroupAbilityId;
				temp.current_rank = aatemp->CurrentRank;
			} else { // this branch is only entered for AAs we don't have any ranks of
				temp.id = aa->Index;
			}

			// Only add the entry if we're not maxed
			if ((!temp.max && temp.current_rank < temp.rank) || (temp.max && temp.id != -1))
				aa_list.push_back(temp);
		}

		NextKey:
		// Now we get our next key, this while loop will either leave us at the start of the next key or ...
		while (*key++) {}
		if (!key[0]) // the secondary null that terminates
			break;
	}

	return;
}

VOID cmdAapurchase(PSPAWNINFO pChar, PCHAR szLine)
{
	char szArg[MAX_STRING] = { 0 };
	GetArg(szArg, szLine, 1);

	if (szArg[0] == '\0' || !_stricmp(szArg, "help")) {
		WriteChatf("-- MQ2AAPurchase --");
		WriteChatf("/aapurchase help -- List commands, aka this");
		WriteChatf("/aapurchase now -- start buying now");
		WriteChatf("/aapurchase add \"AA Name\" rank -- M for max instead of a rank");
		WriteChatf("/aapurchase bank # -- set the amount of points to bank");
		WriteChatf("/aapurchase load -- Reload INI");
		return;
	}

	if (!_stricmp(szArg, "load")) {
		WriteChatf("MQ2AAPurchase. Reloading INI.");
		LoadINI();
		return;
	}

	if (!_stricmp(szArg, "now")) {
		WriteChatf("MQ2AAPurchase. Starting purchases.");
		g_autopurchase = true;
		return;
	}

	if (!_stricmp(szArg, "bank")) {
		GetArg(szArg, szLine, 2);
		if (szArg[0] == '\0') {
			WriteChatf("MQ2AAPurchase. Turning off banking AAs.");
			g_banked = 0;
		} else if (IsNumber(szArg)) {
			g_banked = atoi(szArg);
			WriteChatf("MQ2AAPurchase. Will now bank %d AAs before purchasing.", g_banked);
		}
		char szTemp[255] = { 0 };
		snprintf(szTemp, sizeof(szTemp), "%d", g_banked);
		WritePrivateProfileString("MQ2AAPurchase", "BankPoints", szTemp, INIFileName);
		return;
	}

	if (!_stricmp(szArg, "add")) {
		GetArg(szArg, szLine, 2);
		if (szArg[0] == '\0') {
			WriteChatf("MQ2AAPurchase add usage:");
			WriteChatf("/aapurchase add \"Name\" level");
			WriteChatf("Name is the name of the AA, level is to the rank you want or M for max");
			return;
		}

		PALTABILITY aa = GetAAFromName(szArg);
		if (!aa) {
			WriteChatf("Couldn't find AA by the name of %s", szArg);
			return;
		}
		AAEntry temp;
		temp.clear();
		temp.name = szArg;

		GetArg(szArg, szLine, 3);
		if (szArg[0] == '\0' || szArg[0] == 'M' || szArg[0] == 'm')
			temp.max = true;
		else
			temp.rank = atoi(szArg);

		aa = GetMaxOwned(aa);
		temp.current_rank = aa->CurrentRank;
		temp.id = aa->NextGroupAbilityId;

		char szKey[12] = { 0 };
		snprintf(szKey, sizeof(szKey), "%d", aa->ID); // we use the group ID by default
		char szValue[MAX_STRING] = { 0 };
		snprintf(szValue, sizeof(szValue), "%s|%s", temp.name.c_str(), temp.max ? "M" : szArg); // szArg is still rank in this case

		WritePrivateProfileString("MQ2AAPurchase_List", szKey, szValue, INIFileName);
		aa_list.push_back(temp);
		return;
	}
}

PALTABILITY GetMaxOwned(PALTABILITY aa)
{
	PALTABILITY temp = NULL;
	if (HasAa(aa->Index)) // We have this AA
		return aa;
	// We don't have the first AA we found in the AA manager, lets find the highest we own
	temp = aa;
	while ((temp = pAltAdvManager->GetAAById(temp->NextGroupAbilityId)))
		if (HasAa(temp->Index))
			return temp;
	// We looped over all the AAs the manager knows about and don't own any! must not own it
	return NULL;
}

void __stdcall GainedAa(unsigned int ID, void *pData, PBLECHVALUE pValues)
{
	if (!g_auton)
		return;

	// You maybe thinking that just checking AAPoints in CHARINFO2 would be faster, which it would be
	// BUT the message arrives BEFORE that is updated, so checking that is useless here.
	int aas = 0;
	while (pValues) {
		if (!strncmp(pValues->Name, "pnts", 4)) {
			aas = atoi(pValues->Value);
			break;
		}
		pValues = pValues->pNext;
	}

	if (g_banked > aas)
		return;

	g_autopurchase = true;
}

void __stdcall GainedLevel(unsigned int ID, void *pData, PBLECHVALUE pValues)
{
	if (!g_auton || g_banked > GetCharInfo2()->AAPoints)
		return;
	g_autopurchase = true;
}
