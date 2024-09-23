/**
 * vim: set ts=4 :
 * =============================================================================
 * SourceMod Sample Extension
 * Copyright (C) 2004-2008 AlliedModders LLC.  All rights reserved.
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, AlliedModders LLC gives you permission to link the
 * code of this program (as well as its derivative works) to "Half-Life 2," the
 * "Source Engine," the "SourcePawn JIT," and any Game MODs that run on software
 * by the Valve Corporation.  You must obey the GNU General Public License in
 * all respects for all other code used.  Additionally, AlliedModders LLC grants
 * this exception to all derivative works.  AlliedModders LLC defines further
 * exceptions, found in LICENSE.txt (as of this writing, version JULY-31-2007),
 * or <http://www.sourcemod.net/license.php>.
 *
 * Version: $Id$
 */

#include "extension.h"
#include <chrono>
#include <thread>
#include <CDetour/detours.h>

// Global singleton for extension's main interface.
FpsLimiter g_FpsLimiter;
SMEXT_LINK(&g_FpsLimiter);

CDetour* g_EngineFrame;
IServerGameDLL* g_ServerGameDLL;
CGlobalVars* gpGlobals;

DETOUR_DECL_MEMBER0(CEngine__Frame, void)
{
	static std::chrono::high_resolution_clock::time_point start;

	// We need to account for the acctual frame time.
	const double sleepTime = gpGlobals->interval_per_tick - std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - start).count();
	
	// Precise sleep.
	const double accountedError = 0.002; // in seconds
	const auto sleepStart = std::chrono::high_resolution_clock::now();
	std::this_thread::sleep_for(std::chrono::duration<double>(sleepTime - accountedError));
	while (std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - sleepStart).count() < sleepTime)
		continue;

	// Call the CEngine::Frame function.
	start = std::chrono::high_resolution_clock::now();
	DETOUR_MEMBER_CALL(CEngine__Frame)();
}

bool FpsLimiter::SDK_OnLoad(char *error, size_t maxlen, bool late)
{
	// Get game data.
	IGameConfig* gamedata;
	if (!gameconfs->LoadGameConfigFile("any_fps_limiter", &gamedata, error, maxlen)) {
		gameconfs->CloseGameConfigFile(gamedata);
		return false;
	}

	// Create the detour.
	CDetourManager::Init(g_pSM->GetScriptingEngine(), gamedata);
	g_EngineFrame = DETOUR_CREATE_MEMBER(CEngine__Frame, "CEngine::Frame");
	g_EngineFrame->EnableDetour();
	
	gameconfs->CloseGameConfigFile(gamedata);
	return true;
}

void FpsLimiter::SDK_OnUnload()
{
	g_EngineFrame->Destroy();
	g_EngineFrame = nullptr;
}

bool FpsLimiter::SDK_OnMetamodLoad(ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	gpGlobals = ismm->GetCGlobals();
	return true;
}