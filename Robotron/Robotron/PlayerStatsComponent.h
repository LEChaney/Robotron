//
// Bachelor of Software Engineering
// Media Design School
// Auckland
// New Zealand
//
// (c) 2017 Media Design School
//
// Description  : Per player health and score info.
// Author       : Jack Mair
// Mail         : jack.mai7246@mediadesign.school.nz
//

#pragma once

#include "PlayerInfo.h"

#include <memory>

struct PlayerStatsComponent {
	PlayerInfo playerInfo; // the username, score, lives.
	int extraLifeTrack; // Keeps track of the players score until they get a new life. Refreshes when they do.
	int extraLifeThreshhold; // How many points the player needs to get another life
	bool isRespawning; // Is true when they are dead and will respawn
	float respawnTime; // The time it takes for the player to respawn
	double invunTimer; // Players are invun after spawn
	double lastSpawnTime; // The last time the player spawn in, used to check invun timer
	double deathTime; // The time it takes for them to respawn
};
