#pragma once

namespace Weltall
{
	class FGameState;
}

namespace Weltall::Testing
{
	/**
	 * Makes every character and gear of a game state far stronger than the story's battles ask for: full points at
	 * their caps, attack, defense and ether well above any enemy's, a gear's armor, output and fuel likewise. A
	 * battle fought by a random pad is then won, and the play goes on past it; what the story's scripts read (who is
	 * in the party, what it carries, what it has done) is left as it was.
	 */
	void StrengthenParty(FGameState& State);

	/**
	 * Elly back in the party as a clear file has her: joined and in the menus, in Regurus, in the second slot where she
	 * is not in the party already (Fei leads), the game a clear game's.
	 */
	void BringEllyBack(FGameState& State);

	/**
	 * The game a Keep Elly game's (KeepElly.h), its rule applied, and Elly in the second slot where the rule has her back
	 * and she is not in the party already: the stretch from Merkava on played with her, as a player who brings her in.
	 */
	void KeepEllyInParty(FGameState& State);

	/** Fei drawn as Id, the game a New Game+ where it is no clear game already. */
	void DrawFeiAsId(FGameState& State);
}
