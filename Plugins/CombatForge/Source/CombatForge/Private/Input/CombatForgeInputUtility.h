// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/CombatForgeInputTypes.h"

namespace CombatForgeInput
{
	inline uint16 NormalizeDirectionalBits(uint16 DirectionBits)
	{
		bool bUp = (DirectionBits & static_cast<uint16>(ECombatForgeInputToken::Up)) != 0;
		bool bDown = (DirectionBits & static_cast<uint16>(ECombatForgeInputToken::Down)) != 0;
		bool bForward = (DirectionBits & static_cast<uint16>(ECombatForgeInputToken::Forward)) != 0;
		bool bBack = (DirectionBits & static_cast<uint16>(ECombatForgeInputToken::Back)) != 0;

		if (bUp && bDown)
		{
			bUp = false;
			bDown = false;
		}
		if (bForward && bBack)
		{
			bForward = false;
			bBack = false;
		}

		uint16 Result = 0;
		if (bUp)
		{
			Result |= static_cast<uint16>(ECombatForgeInputToken::Up);
		}
		if (bDown)
		{
			Result |= static_cast<uint16>(ECombatForgeInputToken::Down);
		}
		if (bForward)
		{
			Result |= static_cast<uint16>(ECombatForgeInputToken::Forward);
		}
		if (bBack)
		{
			Result |= static_cast<uint16>(ECombatForgeInputToken::Back);
		}
		return Result;
	}

	inline uint16 NormalizeStateBits(uint16 StateBits)
	{
		return static_cast<uint16>(
			(StateBits & static_cast<uint16>(~DirectionMask))
			| NormalizeDirectionalBits(StateBits & DirectionMask));
	}
}
