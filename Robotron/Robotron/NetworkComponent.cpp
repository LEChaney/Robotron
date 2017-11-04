#include "NetworkComponent.h"

#include "PlayerInfo.h"

NetworkComponent::NetworkComponent()
	: id{ -1 }
	, isNewEntity{ true }
	, priority{ 0 }
	, lastInputReceived{}
	, receivedInputThisFrame{ false }
{
}
