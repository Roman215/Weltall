#include "Support/GameDiscs.h"
#include "Support/MenuOracleRun.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;
using namespace Weltall::Testing;
using namespace Weltall::Testing::MenuPad;
using namespace Weltall::Testing::MenuCardSaves;

// The French translation's menus, frame by frame, against its own Menu modules run whole under the oracle harness on its
// first disc (WELTALL_PATCHED_GAMES; skipped without it): the scenarios of the game's own menu tests.

TEST_CASE("The French translation's Enter Name edits the old name at a cursor, as its program does", "[Content][Menu]")
{
	// Fei's name: letters inserted before and after the cursor's moves, deletions after it, the name full, Start.
	FMenuScenario Scenario{"name_fei", EMenuMode::EnterName, 0,
		{{0, 60}, {Left, 2}, {0, 4}, {Left, 2}, {0, 4}, {Left, 2}, {0, 4}, {Cross, 2}, {0, 6}, {Up, 2}, {0, 4}, {Cross, 2}, {0, 6}, {Right, 2}, {0, 4},
			{Right, 2}, {0, 4}, {Right, 2}, {0, 4}, {Right, 2}, {0, 4}, {Right, 2}, {0, 4}, {Right, 2}, {0, 4}, {Cross, 2}, {0, 6}, {Down, 2}, {0, 4},
			{Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Cross, 2}, {0, 6}, {Circle, 2}, {0, 6}, {Circle, 2}, {0, 6}, {Cross, 2}, {0, 6}, {R1, 2}, {0, 4}, {R1, 2},
			{0, 4}, {Cross, 2}, {0, 6}, {L1, 2}, {0, 4}, {Cross, 2}, {0, 6}, {Down, 2}, {0, 4}, {Right, 2}, {0, 4}, {Right, 2}, {0, 4}, {Right, 2}, {0, 4},
			{Cross, 2}, {0, 6}, {Cross, 2}, {0, 6}, {Cross, 2}, {0, 6}, {Cross, 2}, {0, 6}, {Cross, 2}, {0, 6}, {Cross, 2}, {0, 6}, {Cross, 2}, {0, 6},
			{Up, 2}, {0, 40}, {Start, 2}, {0, 60}},
		900};
	CheckFrenchMenuScenario(Scenario, "b5b52c5c",
		"fc2ad6bc c409a8ff 4e51a324 f42a27f7 8e3c4f18 750868c2 67c2a35c a272096b 746fb55c 253555d6 eab7576c a46122fe 9a86ffc8 29b461f2 "
		"630d213f 319010bd e5c40552 d86fcb08 cf298c9e a311fba1 34d5d86e 6eb1d4ba a6c5c836 4a8c08b5 9f63217e 5c0f6b90 b1b3f239 5768f821 "
		"880d8b0c 91b56ffa 6f9e809d 59fb01ec 67a3d3b2 a71fabcc cd3513ca 6330c09a 8977816b d72075b6 080a8d0e 9e45a775 65bdb6c9 ccae78a4 "
		"3e3782fb 1b20a6c3*18 12bc6fe2 8168b43d*5 40888622 5a71e084*4 13522282 36e1ab3a 9d142918*6 28597e27 9d142918*6 54f80e97 72bc7437*6 "
		"be4ede85 72bc7437*6 d770e6df 2a8d35e6 919ecf0e*4 54b545fa 3e67deca*5 117a33c2 1b0132f2*5 749e3969 60a03d98*5 7fbc2c3b e5419b59*5 "
		"944e9af6 85880ab3 d80a2a55*5 4f952953 d80a2a55*6 e0ae34e7 ea34e115*5 6a568869 1ca9b231*5 762b95ae 1a713d0a*6 6fdcec12 7f37138e*6 "
		"657210f0 fe1a85d4 7f37138e*6 657210f0 78996b41 7f37138e*7 7930931a 53019be1*5 1a713d0a*14 87a6b468 1a713d0a*13 fac073f7 1a713d0a*2 "
		"c0709565*4 9c2dac7f 97e74bb3*5 6d4e60fd 199ab986*5 0af3d43f 168ff229*5 a9da418e 7fb87f6a*6 2164d502 dcdb92d5*7 05cd6db2 dcdb92d5*7 "
		"81c89ed0 dcdb92d5*7 81c89ed0 dcdb92d5*7 158c741f 854141c3*7 158c741f 854141c3*7 158c741f 854141c3*6 3aaa6c1c 92405e4e*6 "
		"f752c3c4*31 92405e4e*5 3440d7a1 e34c42bb d38da74d db87d9e5 7b196a36 628b3911 6bbb5733 6a9b70b1 38a5b766 1f3a6567 92edc177 e7bc06a9 "
		"2ceb3af5 d2127e21 f0c45f58 219c2955 6b259172 5f854210 ad7c6a58 24ddaa35*5");
}

TEST_CASE("The French translation's Enter Name deletes the glyph after the cursor and takes the rest, as its program does", "[Content][Menu]")
{
	// Weltall's name: its first glyph deleted, Start (the rest is taken).
	FMenuScenario Scenario{"name_weltall", EMenuMode::EnterName, 11,
		{{0, 60}, {Circle, 2}, {0, 6}, {Start, 2}, {0, 6}, {Cross, 2}, {0, 6}, {Up, 2}, {0, 4}, {Cross, 2}, {0, 6}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 60}},
		500};
	CheckFrenchMenuScenario(Scenario, "5243a600",
		"fc2ad6bc c409a8ff 78d4022a 9e1ce53c 8683364e 806f6e96 e41ba8e1 261ae36f 251863fb 13b95e87 5d85b2d1 a73d9867 5ffc598a 0f3bfbec "
		"65117c60 870c2c6d 40ab19c4 f7b25e0b 9c18a4a9 7226a8fe a9b33208 5229a09f 4056dc08 f329ff37 d783fb1c 08d8fc33 07657362 5734c08d "
		"fb1a3d38 07a93e4e ffd937ae 224886ac fc79c426 ce7b6607 24a75c73 38fb0bc9 d8fdad41 87a8bfc9 7be88284 31c05959 5b89b2c0 4ce061ba "
		"fdb08f4f fd3b3309*18 419a4d3c 67b08476 fd3b3309*7 cea7989c ecc0e57f 3ea3b706 09016b2a e63fdb6f d1a98885 abbc64d5 6f9d5226 09fea8b2 "
		"792e0621 c91fe8bd 75810b44 edf3d51b adddc906 e432a340 a9d9bf23 3ccfe0a5 e55fa65d ad7c6a58 24ddaa35*5");
}

TEST_CASE("The French translation's Enter Name takes a name as it is, as its program does", "[Content][Menu]")
{
	// Crescens' name confirmed as it is, the grown Emeralda's portrait shown.
	FMenuScenario Scenario{"name_crescens", EMenuMode::EnterName, 20, {{0, 70}, {Start, 2}, {0, 60}}, 300};
	Scenario.Pokes = {{0x1d30, {0xff, 0x7, 0xff, 0x7}}};
	CheckFrenchMenuScenario(Scenario, "933fcef7",
		"708d3280 c409a8ff c62d49c9 9e1ce53c 8683364e 806f6e96 e41ba8e1 261ae36f 251863fb 13b95e87 5d85b2d1 a73d9867 5ffc598a 0f3bfbec "
		"65117c60 870c2c6d 40ab19c4 f7b25e0b 9c18a4a9 7226a8fe a9b33208 5229a09f 4056dc08 f329ff37 d783fb1c 08d8fc33 07657362 5734c08d "
		"fb1a3d38 07a93e4e ffd937ae 224886ac fc79c426 ce7b6607 24a75c73 38fb0bc9 d8fdad41 87a8bfc9 7be88284 31c05959 5b89b2c0 4ce061ba "
		"fdb08f4f fd3b3309*29 ed3a1d41 ecc0e57f 3ea3b706 09016b2a e63fdb6f d1a98885 abbc64d5 6f9d5226 09fea8b2 792e0621 c91fe8bd 75810b44 "
		"edf3d51b adddc906 e432a340 e52109fd 3ccfe0a5 e55fa65d ad7c6a58 24ddaa35*5");
}

TEST_CASE("French translation: The Abilities screen heals with an Ether until its EP runs out, as its program does", "[Content][Menu]")
{
	// Fei's InnerHealing on himself and on Citan until his EP runs out (the window closes); entries not learned or not
	// usable here; Citan's list by R1, his Sazanami on a party already whole (the buzzer).
	FMenuScenario Scenario{"abilities_use", EMenuMode::Normal, 0,
		{{0, 45}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Cross, 2}, {0, 60}, {Right, 2}, {0, 6}, {Cross, 2}, {0, 20}, {Cross, 2},
			{0, 10}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 10}, {Cross, 2}, {0, 10}, {Cross, 2}, {0, 10}, {Cross, 2}, {0, 10}, {Cross, 2}, {0, 10}, {Down, 2},
			{0, 6}, {Cross, 2}, {0, 10}, {Left, 2}, {0, 6}, {Up, 2}, {0, 6}, {Cross, 2}, {0, 10}, {R1, 2}, {0, 12}, {Cross, 2}, {0, 20}, {Cross, 2}, {0, 10},
			{Up, 2}, {0, 6}, {Cross, 2}, {0, 10}, {Circle, 2}, {0, 12}, {L1, 2}, {0, 12}, {Circle, 2}, {0, 60}, {Circle, 2}, {0, 40}},
		700};
	Scenario.Pokes = {
		{0x16c2, {0x0, 0xe0}},
		{0x1702, {0x0, 0x80}},
		{0x2b8, {0x14, 0x0}},
		{0x400, {0x3c, 0x0}},
	};
	CheckFrenchMenuScenario(Scenario, "85a02989",
		"3f4c6d55 ef6008a9 2f8131f1*2 8da80b4e*2 54563e20*2 58e78bbf*2 9025e274*2 ae8873d3*2 c7bd5065*2 35a26cfc d1aec801 e74e118c c2c579cd "
		"10168aaa 3c2a853b 91f12d08 856ec9e6 94b0e738 fdb36462 4b289ae6 9c1439d6 4d8a0771 530ec92e 71bed2f1 b4a08fed 203fd310 6c2a2c4a "
		"1e02ac99 ac6131d0 ed6552a8 e3d77943 19cdc6b7 84ff2f54*7 5190d3d0 a271a096*5 92124cfc 0a76002d*5 2e1c69f5 08ab5233 aca2446a*4 "
		"78629193 a342b25b 558398f6 a5cdfbbf 5377c01a 37f20e65 7ae6d10b 2ebc1d8f 5332e3c0 50496246 56176d6a 76c0dc25 69fa34dd c787a23e "
		"89a9804d d6de7d2f dc2eceaa 5f0629a8 b2fd1161 66c9f68b a2dfe644 66441a37 42a483ca 7ac4e309 f93bd6a9 c3542ddf fddcc2da 59a6f6b3 "
		"b59cb084 e7d72938 d26c068c 02481742 83d44512 c17ffe24 153ec889 9186614c 5aba4cb7*6 9c477858*6 8c0b9b16*6 281a5ae1*6 9186614c*2 "
		"38d9d327 9186614c d317a1c1 6c65b485 0fa668ea*4 50a039aa 2b29b741 e3c89733 d44b30a0*19 25b29ad4 18b74eb2 582e2922 3745ce30*9 "
		"a2d7e7bc 3745ce30 0a322498*6 5d6be462 1e71ccc1 421d995b a57578fb*9 61dccebb 2266c689 84ed312d 2a893ca6*9 b7ceee9f 3bcb48ff "
		"aeb04e77 30c0b922*9 d8424392 4aff2df4 100bd3fa 681d77f0 bcd9ff94 2cf6b09b*5 585d6e8c*2 1233479f 585d6e8c*3 92037292*6 7d66d0be*2 "
		"746e1e43 7d66d0be a571c93e*2 4ba75455*4 e4d5b50b 4ba75455 504dbd3f*6 93830545*4 fa73a290 93830545 7df44002 f488dd79*5 781b0c4f "
		"dde7a6b3 fe6b49ea 3b468497*3 1862a125*2 25f46865 1862a125*3 d505b85d*6 0a9ec1a3*2 7a600cb5 0a9ec1a3 7b893ebe 4e7fb8c4 7fbfb9ac*6 "
		"aed39f2a*4 bb7d7f2c 93be6df9 8f3f2513 aabc9eec*19 71f38485 b6d2b909 8f3f2513 aabc9eec*9 91ff254b aabc9eec e2540cf6*6 8b08a214 "
		"f107281f fb8e7e87 e2540cf6*9 d24e4061 1286c4e1 4f4aa89c 4f2e0b6b 20d2f28e*5 b7f52fba*5 4ece26cf 4e7fb8c4 c89c7545 0a9ec1a3*4 "
		"c85c0f61*6 3b468497 a4500f3d 44c8a690 c7798c45 aa610e40 54c26010 7dd0acc3 c5e6eeca 7675d7d0 3c364f1f 8efccb2a 01ffb8db 9fb1b704 "
		"bbeb74fd 6d50b1aa 32eeafac a663596d 6628060e 7d99b3b7 371e863d 7348e8bf 9e691c0d 47ea5c0a ab0d78aa f867bcf4 c97d028b*16 "
		"071b1666*22 f33fb45b ad9eaccb cba6170a 4b605b5e afd41ec7 f2e9546d 263528d2 b0d58ed4 698f918b 5dede884 9b5b47ab a48834d4 e9d658f5 "
		"995a7db1 6323cda6 6293445b 80f0727f 4dfa8acd 7d84a411 fb728677 da459620 ca0ba63a f033b6d6 a1a1d77d*2 24ddaa35*3");
}

TEST_CASE("French translation: The Equip screen changes Billy's ammunition and his gun, turning his pages, as its program does", "[Content][Menu]")
{
	// Billy's second page: a load changed while its type's count is full, another while partly spent; then his gun,
	// which returns its load and opens the second page's list at once.
	FMenuScenario Scenario{"equip_billy", EMenuMode::Normal, 0,
		{{0, 45}, {Down, 2}, {0, 4}, {Cross, 2}, {0, 60}, {R1, 2}, {0, 20}, {Right, 2}, {0, 8}, {Cross, 2}, {0, 10}, {Down, 2}, {0, 6}, {Cross, 2},
			{0, 12}, {Down, 2}, {0, 6}, {Down, 2}, {0, 6}, {Down, 2}, {0, 6}, {Up, 2}, {0, 6}, {Cross, 2}, {0, 10}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 12},
			{Left, 2}, {0, 8}, {Cross, 2}, {0, 10}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 14}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 12}, {Right, 2}, {0, 8},
			{L1, 2}, {0, 20}, {Circle, 2}, {0, 60}, {Circle, 2}, {0, 40}},
		620};
	Scenario.Pokes = {
		{0x1d34, {0x0, 0x4, 0x2}},
		{0x1d30, {0xff, 0x7, 0xff, 0x7}},
		{0x22b9, {0x64}},
		{0x22b8, {0x1e}},
		{0x1d38, {0x1, 0x2, 0x2, 0x1, 0x3, 0x1, 0x1, 0x3, 0x1, 0x1, 0x2}},
		{0x1d9c, {0x20, 0x21, 0x32, 0x33, 0x35, 0x36, 0x38, 0x43, 0x44, 0x39, 0x3a}},
	};
	CheckFrenchMenuScenario(Scenario, "c940374a",
		"0ac9e39d ef6008a9 2f8131f1*2 8da80b4e*2 54563e20*2 58e78bbf*2 9025e274*2 ae8873d3*2 c7bd5065*2 e589ad1c c967af5d 448beff9 e88e27de "
		"3d9301f9 fc57139a 13bfac26 6666cde4 8cf1f93b 28a01d57 687dc58e 1b25279c dbd6083d 59278ed3 d38c520c 6b08dcd7 836a490c 540653ef "
		"d04d8e8d 52e5ca09 14e4c1a3 d2f7f82b 6fc615a5 d25934ec*7 93126a47 1f893ed6*5 01b501be b99aad93 60225fcc 45abe4e0 5715026a 6929cf71 "
		"c587ad61 c011f680 72cff303 27795230 a2cd24c0 3cd87650 e2c44411 f1b24db1 3277da93 1766e95f 29751352 66c4dc02 517b4b72 a8aba82a "
		"e708ae2b cf301d7f 28e8c9ed 8314ffa7 368ba2dc d1b00c0a cfb8bca4 932ffae3 7d891965 2466c833 e42eff33 7d3286bd 02f05040 65990742 "
		"bc68f5d6 083649d6 d99d0351*26 355023fb d99d0351 dd19ccbc 0c74471d*19 4801e8ee 0c74471d e6d8c225 b7905d94*7 5458641e f3d28c03 "
		"6c8c1568 61bf2e14*4 53c097bc*5 55a508f5 0da95085 db32fb50 91e5a01a*4 72f79f8f babe090b e57b3662 30fd09f6 94bbdb78*11 f2ff13d8 "
		"94bbdb78 f64cfdf9 44d40987*5 c995e56e 44d40987 ccbd0603 0f35c080*5 866d1423 0f35c080 39ba6255 bec14f57*5 5e94a907 bec14f57 "
		"ccbd0603 0f35c080*5 b388af68 fc07ac57 a15fbd88 8f88272e 33cf09e9*6 8691cd54*2 4aabadfc 8691cd54 0eacc138 126e3cb7 4a369ad3*4 "
		"20e14abd df610979 a7ed53d0 88552fc5*11 f66d1715 88552fc5 2cbe53ce 13f61ccd*7 bcc45972 a2fbae29 d7ee5d58 48494386*4 8bcbd077*5 "
		"0e99785f 06df50b0 2df2d773 ebdcb3c6*4 4e0e49c6 bdea3c0f d8089e7a fb5efeb5 35ceaaa5 14c4a523 89066b01*6 8e8edcc0*5 1a6d9c2c "
		"8440fd16 49e1a6a3*5 54066f66 cf3f3f01 14eab4d9 fc0726ac 14eab4d9*11 5a8a0eed 14eab4d9 734c8e65 ca796bbf*7 041d1cbe ca796bbf "
		"26deb5c3 d99d0351*19 b1e9c591 e1d70e6c 2c27df1e 31c1be9f 4e713514 05ee6d91 a471dce1 3099dc84 9cb0cff4 2b996c4e 6bc2885f 06d1df26 "
		"b21498ef 6e687af1 7c2c3ea2 e0a53417 b06d5c7a 3316ec1d 75d4d541 e1561b9b fd1e2596 b4bf69a3 e9892e4f fd34d691 ab6c97eb 7bff029d*37 "
		"1c8ccdc3 faf8a445 b8036520 425d73bd e8e2fb05 b04275c8 a942edd8 2a5b951b 6bb10712 263b3e25 1713ecb1 223e6d57 67b22cb1 f4124c81 "
		"24144810 aa6dff5a 029f2a58 3f85caff 33bf90fb 658dcfa5 6feca642 25b29197 9eacec6d cfe89f75*2 24ddaa35*3");
}

TEST_CASE("French translation: The Equip screen opened from the Status screen changes an accessory and returns to the sheet, as its program does", "[Content][Menu]")
{
	// Equip from the Status screen has no flight of its own and the sheet is drawn again after it.
	FMenuScenario Scenario{"status_equip", EMenuMode::Normal, 0,
		{{0, 45}, {Cross, 2}, {0, 120}, {Up, 2}, {0, 8}, {Cross, 2}, {0, 20}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 10}, {Down, 2}, {0, 6}, {Down, 2}, {0, 6},
			{Cross, 2}, {0, 12}, {R1, 2}, {0, 20}, {Circle, 2}, {0, 60}, {Circle, 2}, {0, 60}, {Circle, 2}, {0, 40}},
		520};
	Scenario.Pokes = {
		{0x1e00, {0x1, 0x2, 0x3, 0x63, 0x5, 0x1, 0x1, 0x2, 0x1, 0x1, 0x1, 0x1, 0x1}},
		{0x1ec8, {0x2, 0x3, 0x11, 0x12, 0x5a, 0x5b, 0x5d, 0x5e, 0x62, 0x63, 0x68, 0x74, 0x19}},
	};
	CheckFrenchMenuScenario(Scenario, "fcf0fef9",
		"3f4c6d55 ef6008a9 2f8131f1*2 8da80b4e*2 54563e20*2 58e78bbf*2 9025e274*2 ae8873d3*2 c7bd5065*2 077e39fc 3d6efaa7 daeb231a 10d28ea6 "
		"74e5ef9e 10ed15c4 d1b548d8 d65873be 3dc9787a 03301830 57a99e73 a8f1723f 4856fbae 629c8219 9846603d 8b7a78d6 e076bfa4 168e1db0 "
		"1d76dc8f bfeb454d 6a3a4cb4 7cb00b62 80320c78 e307975f*7 15fac373 3303fc38 b5e8e8ee f94e5120 fe19596a 1a6af0bd d8855f08 42d96700 "
		"5f0eacfe 96b5da70 7b8e0993 0e5c9ab9 c6e04648 01861972 f121e839 3f6625b9 49645981 e1709111 98509aab 31d55008 63f623aa 71418c7f "
		"e68cc794 81701477 27592c6d 6a6fcd90 25b9fdc9 ae537541 01bd674f e0f943af c4cfd8f3 d64bbf43 fa141d0d 6b658684 cd4ff320 c258dd53 "
		"2b00960a 3cac34d8 acdda991*36 7c398721*48 30ed7886 7c398721 61389fb0*8 8d207079 2840aca7 01827eb4 74df865a*19 544c39ef 74df865a "
		"20d49980 b9ee1bde*5 1693d657 b9907b36 9a6df7a4 e1e1e82f*4 7a00c9e6*5 bc28a820 ea24396f 6636f8fe 59f8c5d2*4 f9cf4d93 e0e33146 "
		"f9cf4d93 3a8b388e 6c7a4c7a*2 8bcb1b02*3 68d4d892 cfb63a13 333e56db 6a42bca8*11 cf110743 6a42bca8 f0eef3f4 1a091018*19 7823b4a3 "
		"5b3e05d1 70de4852 05f0614d 702c8ac8 0546412d 702c8ac8*56 e363270f 142b7c6a bad3fe1e fffd0c4e 8eceead8 d11685cd 1832e708 a0337e45 "
		"b576e70c d33fd393 47a197b8 011ecf84 b024b1fb b50308df cf420367 8b8db059 8cae7c0f 0a6a0d62 2d087207 b7f75956 e397576c 00d9ac7f "
		"29d8c58c dd2f5934*3 77966f9a*36 b4d2c986 ce66806a 2a3e944a d4ba033c 0e339bd8 65e7b52d 53342746 1e2c39ff b7e37962 4bb879a4 3ccdf9f7 "
		"3b38ec8d c43811cb dbb403b1 2a730b49 18e1abdc 571c6d97 2a958166 4c274170 3577de11 870f0265 81ef202b c235d98a dfbe9524*2 24ddaa35*3");
}

TEST_CASE("French translation: The title's Continue moves over both cards and loads a save, as its program does", "[Content][Menu]")
{
	// Right to the second card's save, Left back, Down and Up between the first card's; Load answered No, then Yes.
	FMenuScenario Scenario{"file_title_load", EMenuMode::LoadGame, 0,
		{{0, 40}, {Cross, 2}, {0, 70}, {Cross, 2}, {0, 40},
			{Right, 2}, {0, 24}, {Left, 2}, {0, 24}, {Down, 2}, {0, 24}, {Up, 2}, {0, 24}, {Down, 2}, {0, 20}, {Cross, 2}, {0, 30},
			{Cross, 2}, {0, 30}, {Cross, 2}, {0, 30}, {Left, 2}, {0, 6}, {Cross, 2}, {0, 60}, {Cross, 2}, {0, 60}},
		520};
	Scenario.Cards = {{{FirstSave(), ThirdSave()}, {LongSave()}}};
	CheckFrenchMenuScenario(Scenario, "15898266",
		"b4ead516 ef6008a9 503a6645*2 4b77ef5f*2 db3467bc*2 261cb849 3ef16831*32 e951fa68 2c3f221c 40544a35 d9e0a11a 6c1f0eba 0812f6e4 "
		"85c33bdd 7000f648 d4899d32 fca90238 b14e88a1 a06e7914 083b57a7 af37ab8e 8ec30d28 7efbb255 7ff764fa 2ee8a242 f132b542 4849991f "
		"35946729 20a9ce58 821159d0 07d8538b 48f4cde9 b6e7a5d5 47bdca59 e02d2ac4 016df4d1 c0c73858 5a09f133 c7a7e080 56567f8b 1b2f53c2 "
		"3b3e2813 5c0ba11c a91614a4 512bef97*35 c965e71a 212bd8b7 fc9dfe8c 12dfa2ba 7046d5dd ea618d5b 34cd8e5f 2fba4771 423d9197 fa5e25ae "
		"80cd1f2b 9933a1a6 5da8c427 fb3914ac c11d2885 996cf29c c11d2885 fb3914ac 5da8c427 9933a1a6 80cd1f2b fa5e25ae 423d9197 2fba4771 "
		"34cd8e5f ea618d5b 7046d5dd 12dfa2ba 3f38747b 549a7fa2 d2abcd98 12382086 31078ee7 7e84fa4f 7f7492da 3ba18d86 5f7f0963 a7438fc6 "
		"e36c6400 d6eded71 0241e1b3 ec8e022c 9e75c12b 8b4864e6 b4c26c07 ab7e1a86 7ed12043 84076491 7ed12043 ab7e1a86 b4c26c07 a70aa02f "
		"e379649e 96e3107c 95187d4c 3781b4fe 87366c97 69452bd1 6f2acf25 f96475b2 4cfe59b4 f4b96281 f246a2af c297be43 88ee3f29 359e25f7 "
		"57b837f2 cb02994f 6def9aae 2bee4fc5 34cd8e5f 2fba4771 423d9197 fa5e25ae 80cd1f2b 9933a1a6 5da8c427 fb3914ac c11d2885 996cf29c "
		"c11d2885 fb3914ac 5da8c427 9933a1a6 80cd1f2b fa5e25ae 423d9197 2fba4771 34cd8e5f ea618d5b 7046d5dd 12dfa2ba 3f38747b 549a7fa2 "
		"9cd398c5 e0d88c26 b4aeeca6 a31e9eb9 1b73c276 7d4c8962 59283aca 6bedb038 7de2a1af 5b6709db 9c4f0a1f d6a8ea85 26b509d5 93fbc06a "
		"c05f8f41 df6c92a4 224b98c0 5fa40138 224b98c0 df6c92a4 c05f8f41 93fbc06a 26b509d5 d6a8ea85 9c4f0a1f 5b6709db 493344ee a0c63b24 "
		"5f7f0963 3ba18d86 7f7492da 7e84fa4f 31078ee7 12382086 d2abcd98 549a7fa2 3f38747b 12dfa2ba 7046d5dd ea618d5b 34cd8e5f 2fba4771 "
		"423d9197 fa5e25ae 80cd1f2b 9933a1a6 5da8c427 fb3914ac c11d2885 996cf29c c11d2885 fb3914ac af93eb6d 97d0e9d4 b65ec15e 6a6d3492 "
		"045fc7ef 27e96d28 712fd7fc 586be611 61871c2b 9b340797 6f5e1711 4854ce76 bfdbd231 1a41bd95 b4aeeca6 a31e9eb9 1b73c276 7d4c8962 "
		"59283aca 6bedb038 7de2a1af 5b6709db e05341ad 0b9d8bc2 e41ad722 c58a6ccb 6373ebd6 b776e7a0 575bdb25 afc792a3 671e42b8 6665ef91 "
		"d5acee8c ffe15ff8 341c1002 88c0f8ad 35955056 d0d99735 993171dd f63e3300 1cc7972d c51717c1 78b54f21 e4bda02c 9d521f6f 0ed8e559 "
		"47d1ad8f 0862442d 51a27356 eb21c7dd b66f2755 337d6deb aacd2eb1 ff0bb330 ac3eedae 6a6d3492 b65ec15e a258b571 c81b8723 1e0e1e84 "
		"f7319cf4 e9d2b581 f7319cf4 1e0e1e84 c81b8723 5a69c39e b65ec15e 6a6d3492 045fc7ef 27e96d28 712fd7fc 586be611 61871c2b 9b340797 "
		"6f5e1711 4854ce76 bfdbd231 1a41bd95 b4aeeca6 a31e9eb9 1b73c276 7d4c8962 59283aca 6bedb038 7de2a1af 5b6709db e05341ad 0b9d8bc2 "
		"e41ad722 c58a6ccb 6373ebd6 b776e7a0 575bdb25 afc792a3 671e42b8 6665ef91 d5acee8c ffe15ff8 341c1002 88c0f8ad 35955056 d0d99735 "
		"993171dd f63e3300 1cc7972d c51717c1 78b54f21 e4bda02c 9d521f6f 0ed8e559 47d1ad8f 0862442d 51a27356 eb21c7dd b66f2755 337d6deb "
		"aacd2eb1 ff0bb330 45694bac 4a7a9a00 3ac51960 60bf5b2f 5b7711a2 569e50cd bc24843b fb136671 026ace14 1e0e1e84 c110ebd3 0881dc23 "
		"b2cf7bff a93798ca 90202061 e983f513 2519de41 b47a37fc 5dccea75 5570b89b 85b0fdf2 16ad5f87 b8174fa7 edba6132 14d50428 f5841a4a "
		"572f1ee7 047f4f2a 5dcb5e58 380e1aa1 4f3d0a18 5adb9348 df1269fd 85d40185 da8673c8 a489e1ad d3670d2f 707d7194 cac10aea 3cbc2f32 "
		"cac10aea 707d7194 d3670d2f a489e1ad da8673c8 85d40185 df1269fd 5adb9348 4f3d0a18 380e1aa1 5dcb5e58 047f4f2a 572f1ee7 f5841a4a "
		"14d50428 edba6132 b8174fa7 16ad5f87 85b0fdf2 5570b89b 5dccea75 b47a37fc 3d6aa76e a30a55ae 4b1d97cb 1fdf6bb4 78eeabf2 5dfcc3b3 "
		"37ca6d7e 4c83f2f9 cff304b6 c5c0c579 be6b888b 09a4e140*35 24ddaa35*3");
}

TEST_CASE("French translation: The Normal Menu saves into a free cell and closes the File screen, as its program does", "[Content][Menu]")
{
	// Where saving is allowed: Save, a free cell, Yes; the save's slot is the lowest free.
	FMenuScenario Scenario{"file_normal_save", EMenuMode::Normal, 1,
		{{0, 45}, {Cross, 2}, {0, 110}, {Cross, 2}, {0, 40},
			{Down, 2}, {0, 20}, {Cross, 2}, {0, 30}, {Left, 2}, {0, 6}, {Cross, 2}, {0, 40}, {Cross, 2}, {0, 100}},
		520};
	Scenario.PlayTime = 0x2468A;
	Scenario.Pokes = {{0x1D34, {0x00, 0x04, 0x02}}, {0x1D30, {0xFF, 0x07, 0xFF, 0x07}}, {0x2CE, {0x0C}}, {0x55E, {0x25}},
		{0x416, {0x63}}, {0x2B8, {0x41, 0x01}}, {0x2BA, {0xE0, 0x01}}, {0x54C, {0x07, 0x00}}, {0x400, {0xE7, 0x03}},
		{0x402, {0xE7, 0x03}}, {0x1930, {0x78, 0x00}}};
	Scenario.Cards = {{{FirstSave()}, {}}};
	CheckFrenchMenuScenario(Scenario, "bae5c08f",
		"0ac9e39d ef6008a9 2f8131f1*2 8da80b4e*2 54563e20*2 58e78bbf*2 9025e274*2 ae8873d3*2 c7bd5065*2 9e07a9e8 2c25bbf5 2e39f6e7 1736d2ac "
		"057abc08 bdfd6e25 0d302390 343ca407 7055af2d a4ac8c94 2872df90 71b29f16 891d68a8 3cd4ea92 55a6c941 b09396c8 bd0f4521 5bd87d8d "
		"e9e02bd7 88ae7eb2 729cfbcd 0e24880f 2a09d0f6 8a82984a*7 d49ea035 ee45f0a6 6887a7c6 988d6715 794c9c62 804e9976 db99f06e 6821b65e "
		"94925a9d baa17030 07e91fd9 c78cabb3 889d2546 5ef65954 c356a4f6 ea851097 b930906c c757fedb 9c7dcaad 03255dc6 2246d993 a6224c4c "
		"5793c165 80a3200c b7e7fb88 550dc62c e8c356f9 d11908a4 36a588e3 0a569dde 7d4a8318 5c852588 b6a38905 6c4b48cd aa320770 dec72549 "
		"a133d01d e02596bc*75 8f22f6a8 7d622898 ee0489c4 54dbc554 a5695f5e 0a350c78 0da8fc47 f342ab9c 5c401605 8f4e75d6 6baf121e a6b09bc2 "
		"6d95d287 cfa5c6b0 21452070 2b47bb32 e826fe3a b3f160e1 95cf06db a065b2a4 364d91a4 d1a53726 54cf2ab3 648376d7 38cb550c e18a6c1b "
		"fba04a4f dac84806 ab73934a 7f5fa89b 1ead380e fea0bd0f 23f715c4 cd3c5952 cf6a44e6 cd3c5952 23f715c4 fea0bd0f 1ead380e 7f5fa89b "
		"ab73934a dac84806 b55691f7 2a3ce26f e9140a58 ffb3842b 545bdfa1 e5f1a308 4ba95c58 c6dcc756 c0a18b35 e1ee55c1 6acbfa54 905e6f80 "
		"3191b93e b8f4e878 e35af4c6 74ee8a90 d17d8824 891d4dba cec6eac8 b9f76823 8a596648 d022fc26 f2fe9496 52eaf48d 2337a007 33a67623 "
		"936e7792 2125afe2 bbe85990 c79a65ed 2d6d0aba e8eb5855 c655f497 87ac5ee2 cea75ed2 2c4e58b4 0ab12efa 59680fb4 ce4c7ac5 987b7de6 "
		"ed627659 209b5330 4d0039f6 79b204e9 b5d8d447 1ca36ee6 772016d9 729be8a3 9697779c dc67175b 69ec802b 0e186f7b 739a1913 ebbe4e2f "
		"a1645918 2bce3dda 431421b3 2bce3dda 934631df 9c3f81fb 212ec37f 4fcfe06f 2eee9f4d 8eacd351 2851bb9b c775e66c 802e722a ae18200d "
		"0b076407 d5219651 3be27f84 8e649314 455f32b6 217c15e7 c2763990 1c897e73 bad646c4 183c0d39 7e954721 eb8d029b e48510fd b117b5fc "
		"9506f894 55b74de6 8f2bc6f7 3140c3ae 0851603f dfd6e142 40308c0a dfd6e142 0851603f 3140c3ae 8f2bc6f7 55b74de6 9506f894 b117b5fc "
		"e48510fd eb8d029b 7e954721 183c0d39 bad646c4 1c897e73 c2763990 217c15e7 455f32b6 0f21c6b3 7e98e6fb bc4d3154 35980b98 cc6c0ead "
		"8b09b35f 0a26e84a c5df7c5d aaafab77 1b42e695 f671fb2c eae490f3 ddf00b39 9e8727f6 c213afc0 e192d011 485e8754 92d47f11 5b66aa5a "
		"cfbd3476 9b539e08 96ac2e74 a8061c89 6cc6cea2*2 0158a5bd*60 48facabc*60 eef67306*60 3ee653f2*10");
}

TEST_CASE("French translation: The Gear screen equips, repairs and boards a gear, as its program does", "[Content][Menu]")
{
	// Weltall's sheet, a part changed in the gear's Equip, the gear's abilities (none usable here), its own Fix
	// Frame HP (from a Repair Circuit) used twice, boarding and leaving it, then Citan's Heimdal by R1.
	FMenuScenario Scenario{"gear_screen", EMenuMode::Normal, 0,
		{{0, 45}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Cross, 2}, {0, 80}, {Cross, 2}, {0, 20}, {Down, 2}, {0, 6},
			{Cross, 2}, {0, 10}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 12}, {Circle, 2}, {0, 30}, {Up, 2}, {0, 6}, {Cross, 2}, {0, 30}, {Circle, 2}, {0, 30},
			{Up, 2}, {0, 6}, {Cross, 2}, {0, 30}, {Cross, 2}, {0, 20}, {Cross, 2}, {0, 10}, {Cross, 2}, {0, 10}, {Circle, 2}, {0, 10}, {Circle, 2}, {0, 30},
			{Up, 2}, {0, 6}, {Cross, 2}, {0, 20}, {Cross, 2}, {0, 20}, {R1, 2}, {0, 30}, {Circle, 2}, {0, 60}, {Circle, 2}, {0, 40}},
		900};
	Scenario.bGearLocked = false;
	Scenario.Pokes = {
		{0x2184, {0x1, 0x2, 0x1, 0x3, 0x1, 0x1}},
		{0x221a, {0x2, 0x5, 0x2b, 0x2d, 0x31, 0xa}},
		{0x16c6, {0x0, 0xc0}},
		{0x983, {0x5a}},
		{0x9d8, {0xe8, 0x3, 0x0, 0x0}},
	};
	CheckFrenchMenuScenario(Scenario, "8bce199f",
		"3f4c6d55 ef6008a9 2f8131f1*2 8da80b4e*2 54563e20*2 58e78bbf*2 9025e274*2 ae8873d3*2 c7bd5065*2 077e39fc 3d6efaa7 daeb231a 10d28ea6 "
		"74e5ef9e 10ed15c4 d1b548d8 d65873be 3dc9787a 03301830 57a99e73 a8f1723f 4856fbae 629c8219 9846603d 8b7a78d6 e076bfa4 168e1db0 "
		"1d76dc8f bfeb454d 6a3a4cb4 7cb00b62 80320c78 e307975f*7 91e69c42 7c4ac604*5 1cddcdae cca0a267*5 c5488432 fd4dd885 cee2fd9e*4 "
		"33dc0140 dcfdf6ae*5 1b5a2412 8e409eeb 31f72f3e cf5a9d60 cfe897b4 55b2400f 7e119128 dd9b44ec 91f981b8 0e7bac2f cad80bcc 7027d65d "
		"68d6fbf5 fe40643a bc278eeb 5a3c0e47 e12ea029 16043442 de613602 9f81f8cd 84bc5a9d 44608055 a11d2fa9 d663b47f 4dae058e e0c607b2 "
		"dbfe5328 4902e1a4 5e7dce9c e7a404aa bfbb116d 3b73fa13 153eb75a 2e89f147 5bb76f23 3b8e707c b7fc7ff9 3b92948c 6e27c2cd*2 517ea3b9*2 "
		"761aac25*8 9eedb478*32 4629da41 3c970780 e57b2d1f 8df18c02*19 d3ed0cf4 8df18c02 87954205 0572c2f4*5 6afbe449 999d6126 2b51d9c1 "
		"d9721d5f*4 36f5905f*5 689d7501 860b3177 685c91c5 29b95dd7*4 0a7113ab d0f8632f 3e060e69 e3e76a29 3d739cba*11 24c9d72c 33a709e2 "
		"c77b7c03 9d0bb898*2 fa20e5b2 501fc0db*26 11dc0ac6 501fc0db eb4006b3*6 1db0b840 5a9ac5d2 9ae6f0ea 29b3b630*4 7f3e3c57*6 76f6b726*6 "
		"4dc21239*6 e78155cf*6 29b3b630 7fb9e90a 96246183 ea2f4953 703df70a*2 1dbe89bf ed70c155*6 bcd851b3*20 8b9e8914 bcd851b3 6f9f7099*6 "
		"4492395d 0987fb34 50e860a4 fc532e38*4 b495e498*6 a5b3b433*6 76914860*6 310ee939*6 fc532e38 6497ebb5 807d8f4a 4c027321 f3332cfe*19 "
		"963a7836 3b26f4f3 3bdd1ab3 c83e0f2a*9 7785ad38 5cef9575 5b361463 4a5d90b1*9 a19b4b88 1880af7e 3659f241 f18e25ee 8613dd8d*2 "
		"9cd790da*6 3b0569e0 c3149bef c8ad0dd5 effe790b*2 34765498 dded0b9c*26 f51b8137 dded0b9c 8d56c3ee*6 59a56763 a217e2e9 621ad6ed*2 "
		"43b43238 3222c8a8*17 78a059d7 ba856892 2145d978*2 ac9ce872 d8dbb505*17 c9cfd555 d8dbb505 8db5defe 80bcda80 285e6eff fd780a49*27 "
		"f11d6eea 9c69d4a4 044d2c09 e6ee9ef8 513c2213 e1195c63 a14c8d6d 5e642856 c10dc603 8e2b0a32 1a7ed8dd 16a10bcb 928a6725 c95d99b6 "
		"44ea9821 ad337912 e16b6c61 4273a509 86f1998e 42c92c8d 701b7103 8b82b8e0 ce0e8f3b ee373a24*39 a4fe5ddc 1c17d45a aa72e33a 6c7194c5 "
		"780d6a85 ce817aeb 931fb653 24d6dc23 a9e02499 daf2854b 87701006 0e37b3e5 b10611c4 3b90d7be c0c702b8 f021d0b0 1a24625b 01cb2027 "
		"54583ed4 0268c88f 32f1b836 338e49d5 4480a584 9a1366ad*2 24ddaa35*3");
}

TEST_CASE("French translation: The gear shop zooms on its parts and sells them into a cart, as its program does", "[Content][Menu]")
{
	// Parts: the camera zooms with the scanner, units put in the cart and one taken out, the list scrolled, the
	// total accepted with Yes; gear weapons: the cart given up after a No.
	FMenuScenario Scenario{"gear_shop_buy", EMenuMode::GearShop, 9,
		{{0, 40}, {Cross, 2}, {0, 30}, {Cross, 2}, {0, 70}, {Right, 2}, {0, 4}, {Right, 2}, {0, 4}, {Right, 2}, {0, 4}, {Left, 2}, {0, 4}, {Down, 2},
			{0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4},
			{Down, 2}, {0, 4}, {Right, 2}, {0, 4}, {Right, 2}, {0, 4}, {Up, 2}, {0, 4}, {Cross, 2}, {0, 30}, {Left, 2}, {0, 6}, {Cross, 2}, {0, 80},
			{Down, 2}, {0, 6}, {Cross, 2}, {0, 70}, {Right, 2}, {0, 4}, {Circle, 2}, {0, 30}, {Cross, 2}, {0, 30}, {Circle, 2}, {0, 30}, {Left, 2}, {0, 6},
			{Cross, 2}, {0, 80}, {Circle, 2}, {0, 30}, {Circle, 2}, {0, 60}},
		900};
	Scenario.Pokes = {
		{0x1d30, {0xff, 0x1, 0xff, 0x7}},
		{0x1924, {0x50, 0xc3, 0x0, 0x0}},
		{0x2184, {0x2, 0x62}},
		{0x221a, {0x2d, 0x5a}},
	};
	CheckFrenchMenuScenario(Scenario, "70f5a76c",
		"408a818a 29ddf54d 2008ffc6 ad83c3ee cf82b142 561246db 6dfe1786 23173b9e 46ef7b13 8674b857 ffbb1f39 a65deeca 5b97b6d6 a2ebf2db "
		"2df73fcb dee9f99e ab53f801 dc554fe4 9b64e342 0c805b60 31361ab5 890fc65b 7cdff0c7 b448592b 09ad533c ba6b0450 8605f985 65b6e79d "
		"36201c64 4ce50eb5 10e11d49 59ec6ad7 f47ea913 9d98ffec 7e1e85fb ca55b7f5 ea353de9 3b1e3b9c 7b42df24 93b5cd82 403f44a3 3c6c637a "
		"b8e7d20e 39b95080 895dd37c 1765c99f 76b2087d c2b64fed a6393bde 52402c36 1bac29c2 e6a55a04 b6e6061f 7b8aeb57 73d9eab8 9eded3ff "
		"b8d6d985 bdf1ea42 f595826c ac34d4a1 25f5b894 f88d8c17 8ce2dbee b6ebc2b6 dff78306 37c03036 8ef76ef7 bc23eb05 17d59198 3390907d "
		"2784eee8 7d183afc 0b08c503 fbb1a2a8 914734d6 0ff23a75 ccb8c13d 2c011ae4 f9321340 23bfac36 3e8380c4 ff68b443 5ddd413b b2d732f6 "
		"15d2db44 f5dcc575 8834d71f e6eeacdb 742155fe 6c4fb7e0 eff0a0bb 5f56d7b6 393dae3a 4c972200 cb41a121 7248fde2 5fed7b3a be65e0d8 "
		"41e41a31 fc7744d5 ba7a4818 9e06bc5b 7a819f3c 1238a78f bb05d4b6 e16f72f4 b5c1aa77 08c7aa8b 95b61dfa f060c9eb 44682eff 31930f86 "
		"b8cc99b0 2797fbc8 e390ce73 d9fcbbf9 7aec0d43 cd7b3246 4751ab39 da466e4a 5657444d 03e66426 b3bdad33 dccb1837 849dc074 d4d71b94 "
		"6b7bab1a dc643e6b a8ccb75b f8129437 69e34855 91f49f0d 953e55c6 7cdc38d5 8dad7f60 0df4b2ea cc0d4ee7 70abab70 57f28f8a dd2d6a99 "
		"b4bff27f 41e3aa2a 65403673 a3015258 53ae6c17 a50018a2 61780334 5d138dd0 8cf1fa50 9aa8167f 3f0d53fd 7dc9407e fdd5ec7e a6976412 "
		"169304e2 18e50eeb f1283139 89bb6a2d 78e8ffed c7c9c1d1 d94c7cdf 973aa19d c6fb1a24 e399d438 0ae922a1 78596b98 5d9aed55 780de51b "
		"68eab04c d00322de f3d09163 26a115b7 f9d623df 17af6c23 b265e441 a3963bac 60d1766d 3ef17c37 6d760244 2bd969e2 32dbc96f e0df9c97 "
		"0b082052 d41b8774 beb4766a bce5e1e7 c763b2b3 54f78ccc ee86d62d aef69634 42a97d13 e45220f7 7c3bc392 576e19b1 e985dfc1 c1a7d7f2 "
		"5fdd6306 bfc9285d bb0a4f62 63e80c75 8f56385a 8da1712e fd451a3b 1e5617d3 622b51cf 696a6f41 c8f17c45 a4ec50e4 b79563d7 0db348b2 "
		"c2b58679 09ee1c1a 53f90ed2 45f73217 3dbb6c9e ad0dbb40 194a34ae 23538654 1024ae24 1c0cdc5d 6690d690 196c81de a5437b42 e177358a "
		"2cde7135 fa320427 217da8a6 7672c1a8 95cb7ff7 6a818c66 da6360ca a5dad7e4 9002f307 e15cf4ef b5d013ac bff3dff4 44e21bc0 18386ed3 "
		"0941a632 211bb707 f33afc35 f2ff376d 5e30129c e1132418 1331f622 3458b0a3 d96424de e6541a97 8f0d54cc fe3319d4 8a8b7fe3 4c091f49 "
		"bcec0573 c72408ef d47fff4f fe6eb0e2 734a960e 4513d259 c5649933 9c9d1040 6f35f955 37999cab 878c836e abff40d1 80980448 672c1473 "
		"976d0ddd b22de6b1 4bd39e0a 0675edeb 5ce784b8 150e4b22 e68aac0d 998b3655 57f13bf0 c9d58b22 f10affe5 f936da3a 25cac3a6 e672b308 "
		"2e3ec990 e30fe309 9f42b566 c27f6de7 1e8137e1 2243cbfc e2e9eafc 717d17aa 17046f88 8e114301 f00b2d3e aef704cb ed701ef1 ed31dafa "
		"5007ecea 6fb2fe09 5b275c0c a9b2a9b1 84b34c07 f59d330e f635c6b5 0b80a92a 5a8bfd76 cf3c7772 e5abcac7 de748b13 62d10a74 70b722c1 "
		"25ed196e 6f7f726e 2abc0964 00e3714d c614d672 cc95182d cbef67ed f2df9c72 bd11f2eb 2a320b44 e570ab5f 0d9c808f aea7ad60 476245d6 "
		"228eff30 37e03748 daf9a346 95d4d837 bf3844d5 b9034e38 53e57c3c 29f78490 7d1380f1 9f7610f9 facf9ee1 1b96389e 72b2b48d 54ae2bb7 "
		"526f578a a8212881 49a5e458 33fb4306 2f19f4e6 858e0421 332d9b2a 4af59d61 c394ca6f 226d6ff4 1460779c 18baaf55 27880c87 be766be3 "
		"71d183b9 39f678ac 31d00528 94160f07 204054bd 558f41db 66238fb7 6bf1874a 79a5e0b6 14666540 b85190c1 f2f47132 14dcab19 abad5af8 "
		"26eb0144 b72f102f bafe73cf fde25c28 251b47b1 d31b2105 104228ab e09b962b d8632d13 779e168f 52d9f60b eba4eb0f 989a1b44 a45ffcb7 "
		"221a1776 96ab1858 98e4720c b118438f dfe26444 6f8dadae 87863143 9eaec032 4c9f026b 42a86755 8d016ff2 bf537bb4 6196ee90 2e19a9ad "
		"958c2373 e972e080 5815b474 0453949d 743c9a01 56f992aa 6b36f888 b87cf859 c0a4b902 9afd2c3b 22393fe0 44c7e292 c3c991cb b6413258 "
		"3311c026 525ebf8f dba1b4e5 41ef5459 e2838bf1 746894fd f51fd0cb 5142b8fb 4ceb0e86 274cdbe8 08f36809 763b60be 46f4a6a4 b9e35add "
		"af0400f5 e5a4e079 e2eadedb 83cc3345 43266fe4 644b423a 71acc75e d1b1b843 4811e153 1e5a2484 6a99490f 441df3e8 69b1b7cb 36565ee4 "
		"7e018606 66995ed7 0c34725f e571b04f 077e190f 0717c800 70b3e797 64e8e718 b0df7ebb 78a94fe0 05b1d186 89e6aed5 5f937f75 7735a2e7 "
		"2385e36a 3ae6a5cb a3423477 e2350ca5 8d382525 f789730b 3cb099e8 523f0e83 42558028 666ac36e fab9ab84 5b4a135f 830c9cc1 87f20842 "
		"1f9133c4 4b57f671 a9d4cf8e aa1205b0 36eb0475 7c53e9c1 7fa5e3d0 0acafeab 2d7ad199 2388b29c 9b7c4b29 dd5b2bdd e25f67bd 0cc7874f "
		"d7f0b886 d21aeb35 8abd5372 80f7bbe5 a382ee6b 630738ed 1db86f26 0f2f58d7 26645868 b88afbeb 7b1fdabd 40486def 908a2ea1 bd4c3ea2 "
		"bdc74a02 bc5d7405 65d97be6 649e7b29 aa7891cc 5f97215e f92d3c98 7d23a4a6 a207d2e3 6886e61b df9826e8 733b57bb 4a9cc272 abcdc86d "
		"ba9aa574 96cae0a6 23acb166 7326a2fc ca57237e d89dd7fd 63acb5b3 b9733073 79f92533 721b8c65 6158e0b3 5f83a878 bc6eb3b9 9614d984 "
		"6a24bf8d 97b47300 1e1b6b95 47eae872 a706a8c6 4db40065 8cfeeada da532ffb 7eaf2a5e c5b5b97a 5d833ac6 d41487cc fbad5f13 66fc58c9 "
		"908004de 4063848b de4c2883 a0294468 2e682970 d5f63df8 66a80182 dc92fe70 fc4f32d6 428cf23c edec5d65 3cf21050 0bae6108 06cf4567 "
		"b9c9a886 bfbc6bd9 cc849459 117b29ae 460d1741 7036f92c 3279ab73 e362d0ff ea4106cd f455b81a ce1b4256 72d8535c 70b5f6dc 885d96ce "
		"0e0a7eae 50bae800 384e6f76 7d1cde4a 583d8815 b2228991 3c263f48 dbf59d2b 1ca2956a c66916bb 86dc4864 97cf6ec6 44e3e29b 4e2de14a "
		"d7e82e43 9b2bbbfb 109a0f46 d03a3c28 3d23c4e9 5764087e 4cc31ad5 e23b2b6b 0e6f7578 0bc64a55 a6134814 1758fd94 12caa98a f877d771 "
		"815e9173 4fe6d1b8 58d44dd7 41834e3b cacba7a8 f8774bf4 143a1b04 0f307fac 219dd0de fe979ca2 d41d2a6d c802a8cb 0a89c1ce 71478136 "
		"89159ffd a5056470 58df9c44 637781e0 aba93fc2 b3a93cc2 1859a23a 9b8a5d4d c62598d0 c2f4b92b dad0ec59 ff887d35 b66e2ad1 31880bd0 "
		"e617b183 9420a101 59253d96 104b755b 64ac1f44 88c77d00 8d970f3b e3aa2268 08b1ca74 d938e82e 4d621296 281042ae b1a85282 fad09f46 "
		"ad55ca5d 745f8d2b 50637d52 27c12bae 6216bb98 a30216dd dd32ddd5 30ec7c99 e27af535 10703513 b18eb584 b8f32b8b d0f11d34 b7ecc937 "
		"cf7bfdf2 8092f7bc 2802efe8 6d84bcfa 64dd9837 b910b79e f77860d4 4f60dbab f2952c16 7b853135 3f4a67f7 8acf1bb7 49986bd6 47a0d5f8 "
		"890a173f 7b3c8298 7f5577c0 b4f82781 5ffbf388 e04732d5 f46efea3 23cfd8bf 0dae282a e569935f f93f8e21 e199cd3a*3 51c24c5d*2");
}

TEST_CASE("French translation: The gear shop buys the party's parts and gear weapons, as its program does", "[Content][Menu]")
{
	// Parts marked, one unmarked, sold with Yes; gear weapons' marks given up after a No; out by the arc's Exit.
	FMenuScenario Scenario{"gear_shop_sell", EMenuMode::GearShop, 9,
		{{0, 40}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 30}, {Cross, 2}, {0, 70}, {Right, 2}, {0, 4}, {Right, 2}, {0, 4}, {Down, 2}, {0, 4}, {Right, 2},
			{0, 4}, {Left, 2}, {0, 4}, {Down, 2}, {0, 4}, {Right, 2}, {0, 4}, {Cross, 2}, {0, 30}, {Left, 2}, {0, 6}, {Cross, 2}, {0, 80}, {Down, 2}, {0, 6},
			{Cross, 2}, {0, 70}, {Right, 2}, {0, 4}, {Circle, 2}, {0, 30}, {Cross, 2}, {0, 30}, {Circle, 2}, {0, 30}, {Left, 2}, {0, 6}, {Cross, 2}, {0, 80},
			{Circle, 2}, {0, 30}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 60}},
		800};
	Scenario.Pokes = {
		{0x1d30, {0xff, 0x1, 0xff, 0x7}},
		{0x1924, {0x50, 0xc3, 0x0, 0x0}},
		{0x2184, {0x2, 0x3, 0x1, 0x4}},
		{0x221a, {0x2d, 0x5a, 0x2, 0x5}},
		{0x20bc, {0x1, 0x2}},
		{0x2120, {0x14, 0x33}},
	};
	CheckFrenchMenuScenario(Scenario, "47697834",
		"408a818a 29ddf54d 2008ffc6 ad83c3ee cf82b142 561246db 6dfe1786 23173b9e 46ef7b13 8674b857 ffbb1f39 a65deeca 5b97b6d6 a2ebf2db "
		"2df73fcb dee9f99e ab53f801 dc554fe4 9b64e342 0c805b60 31361ab5 890fc65b 7cdff0c7 b448592b 09ad533c ba6b0450 8605f985 65b6e79d "
		"36201c64 4ce50eb5 10e11d49 59ec6ad7 f47ea913 9d98ffec 7e1e85fb ca55b7f5 ea353de9 3b1e3b9c 7b42df24 93b5cd82 403f44a3 528f1099 "
		"8d31f778 c9444ebe 6d4892b5 0b73c67a f47112c0 b42e0870 17914cf2 49a27b91 dd01ae32 c13f0572 981ac677 b05cede0 056cfedc 68cffd18 "
		"02d14440 e275010d b46f9807 ac2dca6c 60afbb7a 81e97c03 4a7ffe93 060be36c 66af8ab2 8545b835 53439f12 abe21b73 824dfee8 4f7445fc "
		"9e7967ba be589b8e 853d85f5 863f8223 508b4c5a 34dafd16 ef5d7ea0 60976a75 4a1e8d3d c38e8c2f 55de86cf 3c80f45f 93b79495 5b2a6c23 "
		"3ecc0939 007d602f 923bdbc2 507de52b c1f51484 68677c3c 666251e6 7e41dbcd 71aa03f8 131fd7df 08d83ce0 0a282719 6186f0fc 88d76301 "
		"009cea3d 8ccf0742 1f599e61 46c22129 b7432953 4f4d46e0 923f6d27 0fc89985 b4f551c8 a25c4d58 b0c15827 86f1201c 42886be2 9d2e3bdb "
		"dc01d211 d21364e2 20bc7fc4 b0ec7e2d 9a4f1ef4 b6eec2ed 253f41b0 60a67756 385dbc4a 183e55a8 8afdde31 26535092 bed7ed2c 0a8b8976 "
		"aa16bab9 125d6223 86fa8342 1befbeda 4d3acb3b b0c1e817 9f678164 a1480c10 8acea536 f4c16f72 b3125237 8be4298f 1a7613d0 c916843a "
		"7fe309cd a10a99c7 d32be433 5c7051a5 b5bad780 580fa2e6 86a52f64 4c6c1574 9ce365e1 6ef2a691 bcffad9f f6d0643a c9370310 7c09c3aa "
		"025268c8 03c9a53f b45153c6 e15ed610 30a59e1d c1dcc75c 23e82e03 91724c10 cf70d7c7 65cf3301 13397fcd b746eacd b7c329a2 c2b79b3c "
		"bd10f97c 69fc0b55 7e6bf408 878e2f38 e8906155 c747b7bd 38435073 7b37a7f2 692b9c73 ceb342e6 9234e029 418c5475 156c1846 e7ea0ff8 "
		"529bc5b6 d8b4fa3d 5d8c1a67 cb643eee 0f3eb464 66e624a8 f66d7d3d 076800ff 23560633 efd8b1c3 7f5ce723 13d73b60 31abe123 7425a147 "
		"942d7578 1672f7d7 a68451de 56837ac4 70fc9384 68eeb00a c5ee7431 44a324ff b57fd7d8 11bf1300 b5fbdd88 e900f5ae 126e132f abae5a57 "
		"041986de fe9f55b3 0a5ad9a4 2acb72b4 cec6fa70 a2614ce2 f047e32b 76bb1220 1d8db55a fddc6c74 90437342 40b500a4 f5e50754 39bf611a "
		"e866a62e ae107c15 c9cccb36 2263d7f8 67d45798 da1972c2 efc6033c 1c650756 ff383630 34a46122 e2531a89 73bcdaa1 69ecebd3 6f4589b1 "
		"6f4783f8 f4bffba7 f1d23274 8f4a9855 abebc343 c2f35f01 d6b2d77f 19c72864 1ebedd94 34df1781 09abe92f 181eeede 907972b9 9177445e "
		"7f36291b a3efed61 81a81b52 1c9a7f7b 9a1189a3 077bd580 e954ab11 f80477c3 fe39eb08 d91ae160 3e975f1a 177e4413 aa6260fb da27c1c2 "
		"7c31f898 7ba5057a 508ddd5b a35e80c9 1c3856c9 0fcbba6d 5ae065eb 8702661c 45dca2dc ef8da602 67b3159d 1bc05121 ce3b0a3b 4e51d523 "
		"fd6c4a87 eb954a54 27eb8e1e a9b105cb 9c12342c 10c90d74 b61c01b1 0e96958a dd1f104e 51c5b281 b472121b b13738e2 8ae22eed eef16cf1 "
		"401fa457 a17506e3 6fef8c08 745d9c33 312f70ea 34457670 6e5186ee e9aa8fee 7bae82f0 8caefbbd 3c7fd3b3 32b8df07 fb2a139d 5192260b "
		"de4330d3 7376a7fd 0b39fddd c8f8e13d 80e00611 2bf023bf cf24cdee 20b21170 94310212 43d42b8f 0998be94 99c82f5c 37b5d3d4 2a3eb284 "
		"b00dd40c fab81f02 e8452be0 d2d58d2b 64c67edc 3374a016 079c504b ae700c74 de7cabb0 186a85f7 3ae3e1d9 6fc9bfdf b0d7e83b 7b648c8b "
		"302018f8 5c016285 b043202b a28299db cdfecc7c caba1649 5ba27a27 f10c154d fde0641e c05d3d6d 4003815e 7f51bce1 6e49bff9 d0da2faa "
		"a16c9c7e 90001318 d2356de5 83bc2472 d46fd72e 12886d13 5c961d06 81c87134 1d2da76c dfc12685 503548bd ae569a29 5ddea283 24051126 "
		"4e253635 b97859d9 5e9c1657 8d0dc0d1 4d6ae2a6 7b0d447d ce957073 26cc6518 7daa7921 2a83831e 0cc6cb56 51aa3831 9a125f84 fab0b113 "
		"e70ee3a4 6c62ee16 b5ac6cfa 44839682 4d83a27b d1114266 122d98ee 21c357da 1fe128f3 a42584a5 8356f13e d052c3d8 ef850896 40b3fe57 "
		"37837191 63e52a56 f4290a28 7c1bb3c3 b5fade1c 4dfa5fbd 6abad74c f544fe28 2780cc66 35153c2b 7911c3eb 8328f345 cd1c3136 94f7bd56 "
		"85cc42be bad9f6b7 f4e77267 6b5b0e88 9b317301 c9166974 70f0ed7e 74d2345a 5b315cc0 a3caf72b 2a671f36 54ba664c aaf630c3 a2d44ed1 "
		"39e1c462 b9982485 c45069d4 b165463b 0b033fb4 97240121 95370897 8e01a73a c7cb5dc4 5c7f65bc 80026892 fe606756 edafb4ee 500ca6e6 "
		"d8e681b6 9b6c9d86 43bb5ef3 70d1e8c4 955fd456 743099ea 4b16c91e 6ac08a35 5cd2ce75 09b31213 98fcf6e0 dfe4bab9 855c5f7b 2dd96412 "
		"d02b9a3a 7b95e9b4 58c83944 fbd0f994 166883fa c7abd7be 7d3f7a2b a6190007 75127c20 0cddaf78 1ba47ad6 ac77cc07 ec8a1d4e 964ed1b2 "
		"0c52d77a 69dd3b09 65fa1d24 08b9762b 961c53b9 368eafb1 1fc45e0d bf77a2e6 3b462648 d1d6bab9 0587a44f a6580b19 146eca08 04097194 "
		"68d90569 42a6890a bd9bc5f1 44ed2cad f967454a 6c7406bc 7bafe23b daf76a43 87f05a51 65e7a948 7e1951d9 4379b2ae 750dcf52 c9b583b9 "
		"c0d58179 adf5b55e 9a720550 6c98b6c1 212669c3 1dfa6315 5df077d9 6e48b29a 40f84364 18dfcd34 ab3f1eaa cc4e8b4c 90f2d265 11e3b8be "
		"2106d2a5 f58341a0 f29f7fd6 a891dc53 3662ef49 a06db3c5 9aa7514d 0ab8fa52 90e6cbd3 2bb68d2c 9133bd07 f7f8e315 b0545b86 041ad02b "
		"978b6bab 1c35b67a 3a412399 ab6da0d0 657d19ea c9d25bc3 ff535e14 65bd2573 408b17dd 64045ed6 b19bd5f1 48b97e7c cbe478c1 df0186c2 "
		"e077d969 8a38e9c5 ab384305 0fc02d31 e205467c 0d207244 28f63799 8d66475d 9f10de6a d75a8988 4ee647e4 c2a602f6 f6213039 b490962a "
		"7582c46d de5c74a8 c504a691 7595bdc7 7d83ea13 045d4d41 3ba70f89 737453a7 d358d5a0 76d94e85 0960b969 92e10bae 522f6b42 10ad73aa "
		"5a03aed4 c45000c5 67ee8297 0fe4513a 8f317824 e6ac972b 9ff5a31c dbd926fe b93c6a7d 236989b1 87cb269c e9f13a8e 883b4615 a9a8030a "
		"92c1f882 28bd0d5e 2eeb3ce4 ee2b189c b8521323 30a25335 da2bed14 eb450d06 16246656 6bb4d4b7 851044c7 af3768bf e33e1ce9 fcd5ddfc "
		"ad70406c 3cf6558c 460a2800 9f27c971 a8a01a6e 5f4cef84 a0bd12a3 813b197d f0a79c93 0c66800a 6d90a165 c5dba673 9076cd56 8fd5b0c0 "
		"d6ca9740 20a03fc6 5225767a 9fc0e460 35b32b9d 850c00d6 01a90f89 62793f51 2cee7ed7 b15cb93e 9a235258 9cd1abcf f18a1645 97670be1 "
		"e8b2fbf8 cde34263 042a11fe 2baa1acd 7a4eef8f 42fc7c51 2eeeff44 db06cbaa 0067982e f94781a8 dd811213 015a7740 5f27f61c 2a0ea03d "
		"1cb0831f 397de916*3 51c24c5d*2");
}

TEST_CASE("French translation: The gear shop tunes up a gear's frame and armor and restores its fuel, as its program does", "[Content][Menu]")
{
	// Elly's Vierge (R1 in the inner arc): a better frame fitted with Yes (not repaired), the same frame again (the
	// warning, then No), armor past the gold (the buzzer), fuel and HP restored.
	FMenuScenario Scenario{"gear_shop_tune", EMenuMode::GearShop, 9,
		{{0, 40}, {Up, 2}, {0, 6}, {Cross, 2}, {0, 30}, {R1, 2}, {0, 40}, {Down, 2}, {0, 4}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 70}, {Down, 2}, {0, 4},
			{Down, 2}, {0, 4}, {Cross, 2}, {0, 30}, {Left, 2}, {0, 6}, {Cross, 2}, {0, 80}, {Cross, 2}, {0, 70}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4},
			{Cross, 2}, {0, 30}, {Cross, 2}, {0, 30}, {Cross, 2}, {0, 30}, {Circle, 2}, {0, 80}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 70}, {Cross, 2}, {0, 10},
			{Circle, 2}, {0, 80}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 60}, {Left, 2}, {0, 6}, {Cross, 2}, {0, 80}, {Circle, 2}, {0, 30}, {Circle, 2}, {0, 60}},
		1100};
	Scenario.Pokes = {
		{0x1d30, {0xff, 0x1, 0xff, 0x7}},
		{0x1924, {0xe0, 0x2e, 0x0, 0x0}},
		{0xaf8, {0x96, 0x0}},
		{0xb20, {0x2c, 0x1, 0x0, 0x0}},
	};
	CheckFrenchMenuScenario(Scenario, "fce5bfcb",
		"408a818a 29ddf54d 2008ffc6 ad83c3ee cf82b142 561246db 6dfe1786 23173b9e 46ef7b13 8674b857 ffbb1f39 a65deeca 5b97b6d6 a2ebf2db "
		"2df73fcb dee9f99e ab53f801 dc554fe4 9b64e342 0c805b60 31361ab5 890fc65b 7cdff0c7 b448592b 09ad533c ba6b0450 8605f985 65b6e79d "
		"36201c64 4ce50eb5 10e11d49 59ec6ad7 f47ea913 9d98ffec 7e1e85fb ca55b7f5 ea353de9 3b1e3b9c 7b42df24 93b5cd82 403f44a3 528f1099 "
		"818edd04 3c30a0f4 9621f434 e8251eb6 8df06990 9f60837d 6b5d45de 97d16575 f4fd3a98 7f724e0f 73afc602 9ed952e1 821035d9 1145d0b2 "
		"3bf659a2 62a29049 90b4b782 06d9cb21 28e5f9bd 966f8401 4a7b3aa9 cf707384 6c67dae1 350c7632 74d9de12 921822a8 288b7e52 aa7684f7 "
		"024bc703 bbd1464a e29184cb 050788dc 6646e1a8 282c992d fb4a99ef 142876ac ee1e1a09 a22eae04 bed75fc0 53caf496 1a2a3a10 913b7a1d "
		"ee834a08 fe6fdc71 0c662888 ad201766 1e3e3da0 f4283fe8 45614d4d 18d63f03 642eef99 da1104d0 76618a3d 54a79c2a 2282f889 9d8ba4a0 "
		"817f0769 971cf89b ba5f6b69 8c760376 69f73b90 3c55b97f 033ec30d 368a72c0 ef5bfa99 6c2fc2ba 71c4f40f 78de017f b5963a6b 263f6b49 "
		"3855ede4 c35d6020 79e090f0 d8f8a294 4b0489c7 3e9ad9ab c3a42c5d a2e98f14 46ca2ada 89b31e06 27cf1196 45cdbfb8 d88733e6 585a69e7 "
		"16d81ff0 854212ef 011efc97 e41a7034 96c94d5d 71eb30ff 4feff15a b4e18072 9d503c5e cd63c653 7e61086b 34d873e9 8c9c2351 7c57f34b "
		"0225d4ee aa0ed55a 8da123c9 dcb34323 147c5386 324e49e5 b84bf0d9 5889096c 45468f5c 79d2d1ba 42865321 e82919a0 2b82b096 34eee0e0 "
		"a1b677d4 2aa9844c f5d72478 e0f17eee 95d9afeb 7210d9a9 9effa9b8 b365ed7e 20da7fdf 84eec722 cb04648a f103295f 47da6c0d 0dde14d1 "
		"028491fc 3c4d4df4 7e4f1868 a6da2590 6d56d506 94f0a69b 5491ca41 52332fdd 242ffce3 6751ea24 3f3f4f30 387750e8 98f2d5ab 396f6c44 "
		"a5216a34 ba43da8e 2dedc723 35e8cef9 fb07f312 278e1b7e e6a18554 cb4eff94 7308062e 75eeed61 46ab3391 d93bcd7f ca2b2142 a8c6c378 "
		"cfa39a6f 6b116628 8081d38d 4d7becb6 e2a11bf5 30bf9d99 d2a37b20 5d52267c 8eb81bde df6f9162 016ab75a 8abfb3cc 7cb7dfda b67dfaea "
		"1b1d003b 70fa4590 df13067e 3a1c0f3f 8f1540f7 a4772e0a 2bfd4637 f9d7c667 4c0e7fba cc73090f 1dc6289e 047566af e0fee5e1 f454965b "
		"c96dab08 29941c83 d14ad765 8525bfca 0fa03867 9a99077b f9a674cf c93fbd15 b2fa0b1c 9cc53c30 a7976823 5ce99f88 b87b874b 0394f58c "
		"5abfe484 1c8272fa 693148bc c9c49ca2 f8b933c1 4ee4eee5 b74dcb03 776b1ce6 b5005130 aab41a8a dcb97653 d9dca72f 04295e73 94c87a8a "
		"c70e9012 24c84b37 4dd5fa18 79a24073 9dca7dea d027c6c8 90fcf30d 6ddeb196 07af13c8 c146d370 9a6f2533 149b9d5a 4480925b d4e6ce44 "
		"51549f19 4b325833 39cb378b f5927bf9 d73aa79e 651f4523 42126947 4f42df7b 386d1b96 80e24077 9e8fac86 6cf8e892 4e91a7b7 2f1ea8b2 "
		"e76b2e8a ce77576b a6f41d00 e434906e 9c826181 c965d16c 661f0c84 465b137f e572294e dc1ca3ad 4e8573ed 9b3f813f 27243a1d 7a408cd4 "
		"25469e36 b42ce7f3 796ab227 9952a810 b122152a a5294658 5268d2da 6597fad6 7d1ca294 35f54312 43599f7c 9b6cdb17 95bcae2e bb66e7f6 "
		"799322b1 168ae90f 0e273a06 f37137c0 e2cf8c1b b747d572 efe9fa6c f4a8dcec 8a987de4 760112eb 3cf73c9a a6f0b53d 53d2f517 0fa62d37 "
		"9045b70b 01ed85df 8bf12290 610d8d5c 85bcd557 282f4321 5b012427 2e04d29c 791c2a42 727b3752 87b0476d 0d7cfa42 88c811d2 cd85692b "
		"7a7ff7c5 44fb7c01 74c7e0ae 0a37e2b3 0f5c281e 7a0a67eb e1fb8e71 18c8bd03 83184423 724f4205 68849a66 d345a132 a0afa313 b2d4cd06 "
		"91d92918 0419c23c 8450c266 dea58796 a5587d18 827d6240 1f0c8eaa 237bb791 796dffaa f856fb3d bf6b937c 2ebf5585 612f2469 e8c46047 "
		"3ae1fd43 4c07f11d cacf5937 cfb5dfcb 45210bac 8edf7d18 4994d0f2 6568ebfa 260986e0 e6e60c20 787832cd 53a13e7b 3b3b3be1 e0c6f372 "
		"3c78e0c9 7f5f469c 868c33a3 570e1b79 6f103b4f 0c0deca3 630b13f2 354f51f4 3f9aadfd 5dab499a c6bcea65 a5b9f0f5 4331de7d fb6f8f32 "
		"e67eaab0 bde1ce43 705fe3f8 4cbf850a 24e2b857 a7fc0b1c 3432f5cc 823f4ebe 0390d0a4 11bc9b86 85e1b9c7 a160f426 0967aff5 d86fe9fc "
		"d562a786 4c080ec4 7c77ada6 0b832dff 1a544a9c 9d758ce7 243d47b6 9e76fbf2 84e901d6 ae22f335 c4cb8111 4add632a ddae87e6 a36dd134 "
		"88dbf5d5 79e65ca2 2d5c24a2 fb0ec38b eca86686 eaf25996 28543715 f77c2876 ee45125c 87a815e6 53b5c30d 70c5e180 55d2369e 918fc946 "
		"d277907b c6bd5938 a8d02741 907ff360 9980df9d e3b7b4df f91977fb 89c07cd2 4e1b0003 6cdb7b2a cad73da2 1696478e 8b3b7445 d899e646 "
		"cbc1de7f 802a39ef 8e4af715 4f54b965 63ca5d23 abd2bc32 b715442b 91dae2c7 0f9a97db 80ebeff9 b85eb556 2b9a4de5 36d539e6 e9beea00 "
		"213cd597 a7253dbc 69ba6cc8 7be97820 57f616f3 7c3f24de 622f356a 137c01f5 7536c0af 946c4f35 c6a8cfa1 7f4a19a9 546ebc35 5d4f1dc8 "
		"25b6b671 ea137595 d64cae39 5e9cebf4 e5f68a35 3f451824 fe4bfd6c 10c4ba3b 6569bd75 8455acdc 83343c93 7eaed458 472193ec b88a0d86 "
		"c813704d f7f95b23 0a9cbfb7 f880b0f8 197330b8 fcc70e56 024eba7e f423301f 6487054a 5e70ed15 3347106c 074c6d35 c45cbbea 20b7487f "
		"281c18be 5521af16 2a5b98d7 77e987a9 c7df59a6 f45de40a fa336807 89e4e185 69440c38 8649b282 2522bc5e e31c8772 31cb328f d2b81312 "
		"856bd59e 7fda56ce 632d5ea7 582225f4 8056f75b 49b393c9 915d53cb 93e53e9c 97b34c4d aa57ac38 8f873061 3bf1c2f4 f86e780c fe42cc1f "
		"b0db3f1d cce9f3f3 1cd70960 8005d49e 7abae7a5 75c0afb4 d3a75b56 7d6f80d3 1a9a7ac4 15340267 e5f593b1 e28a8fb0 4776b517 a861e47c "
		"57042def 42e0eee7 8784c7f6 0d1d0ff2 cfca7227 f7704fa9 f5b3ac3a 40fe0032 40dad594 4f5d2518 9491bde9 1167ffae a29ca55c 8e7a832f "
		"18e58302 ba3b0a70 36ddf4b8 82b422e6 6741c72b 3c8be4ab 67aa0e5b e156a2ac 8e58a0cb fb33f73c 608f02e4 1bebd369 300f4162 8a456c5c "
		"3c5ecfcc b5a9808d cff74259 14970884 1cab29a3 df47e10c 4d0178f5 14d0c131 06b09b27 59c7a3e7 c3d670fe fae23ee6 e6587537 08296bac "
		"eeb98b1c 6f1450b1 9668da6e e39a4637 01aa5e77 041664d7 d430f366 31392094 61779ef8 44bf57c4 bc1ce5be a3fe0a3f c6dbaec8 cc60fd98 "
		"6c38a13d dee42690 70676a30 3939c73c 041cafce b5bd3f23 21cd7a68 198fa677 ddf28eb7 9a37a6bf 201a3bb0 2e9cdab6 b4a6efc7 9f8828e9 "
		"71df8c8b 65fe97b2 684bdf8b ec9123b8 8eda04a4 5a1b02b7 fb7d5e28 a2261f4c 5fb72152 2f7b871c 47264ad3 5ef215a9 20485f24 03d1d55e "
		"0b4a69a9 8678a07a 0e24b09d 065016f4 34040d43 2eaf7b0c 7dc16244 f6bb9ed1 e7bc50ae 58397102 b3e7ad67 100a9cb4 68a112a9 900caca8 "
		"8a2e8acc b51a7948 ec19127f 5f10c917 7185edd7 4e5182d3 65da4244 a743e785 18de38cb 46ea9e95 31dc50fa 4074849d de1079ef c0215e59 "
		"dea0fa85 49ebbeae 8e8f0940 426d1601 9717253a 201edc0d 160ea892 a563f6e6 0141ce4b 9a320232 1d56ebd2 b8f19f44 5083c0bf 732a2181 "
		"c7e9eb27 cb870fd0 c3075727 7664700f 7fb5034b 805220be b587bece 7fd39e5e 5a2eaa1c 16d6b048 0ae8e120 1799d916 0f36e5ef c91c3843 "
		"6f0424aa d1258c8c 8ac41f66 f7d5b585 fbc956f5 fa99dc9f d2f86ffc 6f628346 6f3b043c 21cbd3a4 57f84625 9df831d1 5623a91c c43efbdc "
		"78c1ecb3 6db0509b 7409f2cb 84acbb0c 867810a4 2598ee6d 24f18380 0fea143c 8c98fb27 e0890042 4824b671 dc2cb8a7 4c2b4683 afcf080e "
		"b8bf1f9d 2eb3ff3b 1dee0ab6 cf3602f5 c9d41658 4dcc116b 18314e0c 57d94485 6b5fa8ab ac0ffd13 84e86626 7319e181 5c69df87 fadd9323 "
		"31a30160 b1444270 1d197a23 51b774ac ab0f6ab0 a0537d8b c40fac5f 6ff309d5 fc10e870 0ed8968c 4714ff49 604ff3f6 9576c57e f790a77d "
		"be9a9de0 c4f8bf30 d3cbfa8b 67f624bb c7de0758 db31f9f4 905cfd84 3afaf270 520bd11c a5e4e041 d8b7c1f5 f92cf235 5602abac 333f54eb "
		"d2694263 07a4748c a456cf64 25144f01 140e9548 a61afae7 1eea7673 9ef8467e d026729f 103fdf32 74a33a67 f0d1b0ab 5cef2f78 1dc9b5a0 "
		"5d671494 0492120c 26e08305 e86200a3 4a1b1488 ed955597 d08d6ae0 a98a2354 404d9ffb 6960430e b7d36e30 78103450 0dcaa22f 5a318111 "
		"f8c5f000 01c77496 0440e9be 7488f0da bb5aaf5e 3d83b637 5f0a6113 c1f656e2 5364b9ae 2f8ba13c efca51c6 44e5141e a91ea9e7 0a7e238e "
		"82b0d15e 6cb0dde4 f87cd7d2 56484534 24a00692 e2bce429 d21b8e1b 520dad07 ce5b86de 1b9229dd 1f163158 dea2d6c7 50c3551b 385bade6 "
		"5dc9318e 8bb6dfc9 a6aa792e d1dcc40a d00898e1 2c17c1ad 3039c017 8c2d57dd 3f21d054 bf5b2d7d 81c5b747 8a4bf67b fc8758d6 7939e430 "
		"44976ab8 cb9b811d 7cf9ba93 9d04c704 6ce632fe 0ee30236 64efd080 e66f99eb 6522962a 883c32cf a00fe8aa abf9afa7 6d0fe99e 48796501 "
		"465ebf6b 1d987244 5ddc472b b5d678e4 d564de7b f2e7d53e 16b785cb 003d3d49 376569c9 d8fb2618 43bb0999 703ee8dd 00e65e2d c922eea9 "
		"77281664 047c803d f728c7e1 ec47b43f 70967464 aa87ca52 610c659d a9639ea7 39d84ca6 80c9d60a a16d2059 754f951e e90810e9 3e9c3791 "
		"b0083658 1ce9a5d7 ab959643 dee1922f ce447d9d b0664870 5cf7b691 fd9168cc 0ac6eff6 da08591f 98f35674 975134e4 1dbc2d33 5ecaa698 "
		"2a1219ad 6a8efab1 7d533cb0 c277cb30 260cc3e7 1f49bbcf a19852d4 98d110c7 cd975625 93dc0bab 88bbde35 51b83ca8 d6cd54ea fa0bdb52 "
		"2e377266 2b7b63c8 ad9fab33 de18ff7a 01a9f89f 37272794 87560edd ad380408 d0c42d70 df06f7dc 3fefe60e bad1ea35 4e0d3ccd 1186e3ab "
		"92c65775 4d923ffc 46f53ad9 f550c295 9a78bb8f 0ea2eb10 281a3bd1 3c1cead8 32fc6ac9 4c4ad946 ecbf9cfd 81c1fa5d f521100c c81d4436 "
		"b73682d9 f807b749 7d3d769c bf66ca1e b4374f0e 4d806ff1 dbb699c7 5000869b 80ce5b38 d3e606fb 2c38c2c2 07f701bb 74162f86 5dcbfae8 "
		"72fa07d5 c08995aa a2572c29 947a10c4 739d85b5 415e2dd9 b2960067 2133611a 8fe153b4 55f58ba9 e46745d5 c88d4f9c bf8674b5 6c3ca900 "
		"e06f4738 c345a2f9 4db3caa2 a6265750 70ecf364 cd955d01 b6c24044 7f84dbfa 00ffe134 d292af41 fbd8d91d 2e4a2219 0ce8398e d6e31fb6 "
		"b7726ecb 1081c54f 051fe04b cefcdbd3 3e51e644 f68a8da6 fa396fd4 c554bbef*3 51c24c5d*2");
}

TEST_CASE("French translation: The gear shop restores what the gold buys of a gear's fuel and HP, as its program does", "[Content][Menu]")
{
	// Fuel bought with too little gold, then with none; HP alone restored for nothing; a full gear's notice left
	// alone, then closed by a button.
	FMenuScenario Scenario{"gear_shop_fuel", EMenuMode::GearShop, 9,
		{{0, 40}, {Up, 2}, {0, 6}, {Cross, 2}, {0, 40}, {Cross, 2}, {0, 60}, {Left, 2}, {0, 6}, {Cross, 2}, {0, 30}, {Left, 2}, {0, 6}, {Cross, 2},
			{0, 80}, {Cross, 2}, {0, 60}, {Left, 2}, {0, 6}, {Cross, 2}, {0, 30}, {Left, 2}, {0, 6}, {Cross, 2}, {0, 80}, {R1, 2}, {0, 40}, {Cross, 2},
			{0, 60}, {Left, 2}, {0, 6}, {Cross, 2}, {0, 80}, {R1, 2}, {0, 40}, {Cross, 2}, {0, 150}, {Cross, 2}, {0, 50}, {Cross, 2}, {0, 60}, {Circle, 2},
			{0, 30}, {Circle, 2}, {0, 60}},
		1100};
	Scenario.Pokes = {
		{0x1d30, {0xff, 0x1, 0xff, 0x7}},
		{0x1924, {0x14, 0x0, 0x0, 0x0}},
		{0x9b0, {0x64, 0x0}},
		{0x9d8, {0xf4, 0x1, 0x0, 0x0}},
		{0xb20, {0x20, 0x3, 0x0, 0x0}},
	};
	CheckFrenchMenuScenario(Scenario, "37c6ebed",
		"163f7eab 78cb3ac0 677504a1 89b1ae01 1043f02b 66516366 2ecf7f21 97914342 fd100e5d 7cea42d6 03465924 8d1b75da c9b8efe5 e23bd85a "
		"2ab83f8e fd7cc260 8c35ed8a 641bc5d2 1dfe7b0c bcab5374 a0cc385a ef299209 3f065efd 03aaa577 d47b551e cedc2b36 729ab58a 9f522b67 "
		"7edd34f7 1aff43d3 77daf258 ae0bdeb1 91654692 086d417c 76adb24d e3b1bf5c dff6569e 8ddefeed 5ae5ca5c efa364af 3f8961bd 70971db9 "
		"ba8e3ac3 fb8b06d1 8e56f6ce e1de40f3 286c5ac6 00de47c6 2b26224c 25a21864 e6b5164b 4433a392 39bfa4f5 2f7daca2 b24dfc06 cb840235 "
		"924aa20e 567cfe2b 80aece57 8fd678de 9012459c 85ca312f 6a1b6198 9ae29c4c 1107c64a 9402bf0c 69c16c3b a3a95c92 3aae7cf0 995e3710 "
		"ac22bea8 4ac3a64e 1ffd11e7 4bb87416 45a8fe21 a5d1fada ee1789b5 9c43689c 6eaf03fb 014a6765 ba47f309 67b72e23 f5e718dc e3078e39 "
		"d201bdc8 3fbfc414 1e768c4b 2c9e7d20 8fa034f6 56d01141 fc32dccc 206c928e 3aa43c8b 9fb496e6 089106c6 601cfbfa c830dd7f fa6a3665 "
		"d22d01a2 a187f091 d94b8674 f5dec31f 5cb02c0a 24012caa 65862322 4e49d79d e9d537b1 037bdd61 cfad505b db2078d8 a7990a3b f993bae4 "
		"beb501b9 ed7a3669 8ae9a5f3 db5cc8af ab874c26 dceabc88 5b25ebec a9b5c80d 57bb3bf0 d5aeff9e 485f552e a33ffd66 63f09fff 1aadb823 "
		"a4d381dc 3c0b616b ff39f5a0 01e7613b 87f3636e 05dbd23c 505f61b7 27ce3678 a147be87 31bd459b 8111436b ea1ff0bb 607fd773 def1dc6c "
		"14998ac1 f280dd1b d17f3e68 e2b43045 f83502cb 2e296901 7f933fdc 840d2e50 e701382a ddf70aa0 500b2d6b 30515227 5cad9a77 833fca35 "
		"e51d0b76 a0c53684 cc47f5b9 17682952 975cec22 d32a1f3c df9027c1 a86d6c2a 883a83f6 208ebdfe 36839eb9 0df4761b f59ce1b8 7debc4a2 "
		"546e31ea f945ba85 6c105273 8d920cd3 0026cd05 59f1c0c0 aed6c65a aec664ac 5a1b11f8 03154eb3 29b2804f 7e04fc4b 0586eabd fd0ceaa6 "
		"c53c1073 85e13198 1929af00 b222ceb0 99655c31 11c30dac 1fbf3b12 04f4072e fa9ba408 e5af18b1 415949ea f549ab1f 4b5ab688 e149725b "
		"19c617ee 6a0c1df5 01ada513 a7e794f8 2ad46b96 d42f219e f3cb546b 442612f3 9d2db0ea 263d381d cd502cf4 30a88581 d9e7e233 816d4b2c "
		"5b38fe2c f3684c8c 678fc3ce 608ecf0a 4d9a2201 cc5cb574 b6f1c41b cfd271c5 01904b35 5046bd3d b6970d59 d7ccaba7 504d82e2 0e28c802 "
		"2f9ce911 fb52bacf 687cf1f5 2b74c386 cad1fbf2 df152d27 896eb634 945ee145 b0a2a5fd 3049baea eccabb78 93baacae dfd5190f d34b8629 "
		"3c8a0e6b d0e2ca32 c91be093 3064e6dc 734aa5d4 90a1357b cef511c3 2c43b17c c092daa7 e5486634 b3edf935 99e1bbd8 42e7536e 25959931 "
		"d3081291 a9be6118 7359e638 e76d83f9 1ec494d7 90434e82 43a0e706 69a8b7c6 b8eef132 fcc8fdc9 3c06a954 1521ea07 d50fa523 8dafac14 "
		"130a0b1d cc15a715 bc426bec d2c33f3b af0bf353 e060c655 b390859e 350860a7 5fb8e7ef 814805f8 93577604 a5c638a7 27a2b885 426cd1bc "
		"26213f57 933f3c88 b5f425e4 f7c457bb 01f6040e bb77ec7f 5113359a 4e47b157 ffc2391a 281b8e06 301ee663 e6d2ab1b 42d372a5 79bf9f04 "
		"360883e0 948fa907 aa22a7e7 9cc8852a 6b3da847 db0f7c5e 1d68ef5a 3e506270 58fcb79d 168b02be f28d30f9 03318567 30367965 b7701a15 "
		"d14c7eca ad3da2a3 10044965 2664c9c3 af739263 aabe2757 ae36fa51 3d4aba88 54f1cb1b 092c23c4 85ea5cfa 4488a51e e6444880 0383396c "
		"f2220efa e06cfb1d 7b3b9d92 478b4325 68fc7c11 9a45485f 717c636c 959305c8 00b31a9d 5c201d2b 23440ce0 0ce98be5 9e699bd6 99b849d9 "
		"3429d020 7bd49af7 7c70d56f 337dbd95 3a722a85 9286d21c 3f7a0019 7d7649e6 c2e81122 7c7a4dcc bfc08681 ba8e9add f494c591 e2a9ef05 "
		"ec18e015 55a206ad 775b4114 518eccee aa3d0dd3 fbb25757 edd287c8 9d94d80f ab1d7edd 376b47bd 643a51ad 73c39971 fbded3a8 47e15884 "
		"d30e738b 26500c8a 924dc51b 97cf30e5 53bf88c9 ea344ccb 4e7e427e 6e209f19 a5e62363 7625ad6e 6225bfa4 a7a1fd99 1288ac54 9b650282 "
		"8af6234d a3d00e89 ea35b82c ff5fa46c bdd9c869 36f1c7f9 27936f87 b1ca0dea 57b5839a b45a6000 c27dcd96 6b4f2023 1c65fca4 52bd9323 "
		"6c8651fb 2fc7746b 95fc9a60 1260bbfa 5e939c44 de80a87a d6d9a869 4d779b56 6116931a b5091040 1fea7818 ae350f8a 6517dac2 e0b884fe "
		"995bfd5f 3d25cbb5 2b3aa804 f7588d65 f186ebec 812723f9 110e2af1 407e635d e5c3eeba 36381244 5a1ad47d 2d758d8e 9113ef6c ec44a5fe "
		"c01a9ada 92d24fe5 0f972ae7 450263cf b32a9eb6 e228448c 3b9cf844 b4dd7f58 84c99824 25d3c217 f1fb1c6f ccdd98f2 de068353 5c591a06 "
		"f3f6ca1c cb0491fb 6081ae27 f27243de 1a5bb119 3fc03bfb d2c216c4 c62d5723 7784d44b da0ee0f6 24316065 f5c09054 151ccc1c ba50eb69 "
		"acd1d3ad 086c4c9e b0d9f887 f214a85c 141796d4 191365a2 ffc8cd3e 68c0242a 7dcb1309 4d408574 9da5ccf0 e9048fa7 5b42e126 946b1a09 "
		"7931d80b fb03e360 04349c57 268443e3 6f6c38ba 4e981f42 9557c5a3 194662fb 3e536383 158bec23 ad08d5cd 6b3190b2 669f9ee7 0b0cbaaa "
		"23acff6f 231f2899 164a6ef3 52e9ac45 6cb5877d a1c8b9d9 ac7c4913 5f3f3d45 e94e8841 d3e57d49 5dffe0fb 3fda00fd e3c1c60f 595a2457 "
		"b8b9bedd 7a80b839 1c61943c 0dbc4d2f 1a25906f dc9751d3 cce2256c d1a693e1 aa46788e 9aab06c5 13196137 a1a3a06b 89428f97 9d74f579 "
		"d0023266 7dd367b0 d2e8f44a 6d72cf9f 9ea7d4a4 f8667d07 d8b264d8 2c6e05be a6716377 ad34ebfc e2e7bbed e271d354 957f3d9a da783497 "
		"df210fc8 9fe57b3b f5df9e8e 64deef28 615d2c02 3a092894 7501c4e7 e4a8155d 32027238 63e64aff 07b0d1aa 2ffc6cf3 d96e34ae b11a3d05 "
		"0088738d c72f6a7b 04e8f0d2 d9bd84fa db0506a4 dc382399 1e2cfa95 95dae32a a1ff446e 784b7dd0 c0ce3987 c3ee94e6 2ba524df c433cbc4 "
		"8df50343 538507c1 c6b86957 1f7a1c09 3aa494f1 2ddd1790 010281d0 a153ad86 52e805ff 443981c1 6f0c7357 5704f923 fea912ba 8d422b12 "
		"9434d7fa 772949bd 1488fe45 640010c9 c4bc83bc 9752d7db 96c7e93e 717785d9 6d6468fe 5a2411de 5e882d48 7cb14710 b27710a4 a252a329 "
		"32ce7747 a7ca1435 02e65695 4bbc011e e3bb78f8 6c9c2168 62e7f053 239b1802 29807359 e2b890d6 1cb88ca3 00b7d6e9 b5d0dcc2 4396027d "
		"d3a4dd8e c3748fe2 59ef2867 5d477bda 9237ec37 91b8be21 91f915e3 75a70b28 717adfb8 ce0bd61c da58cbc6 52696294 cdfe887d b486778c "
		"30d7cc01 9c373ef3 d1d419ae cd83458c 082e4738 01e800f3 f3f47cc4 7dd036c0 d378843b 27d4c589 6b5de2bb adb013db bed210d8 2c1f3976 "
		"ccc74b6a c6ae5ad8 bbb43019 6f62cf19 7ab6ea7f f5638d9f 1fca97af 8e6038c7 1657166c 7db94739 85281c7a f03d8e86 991e9dba 04c442de "
		"ad54f972 a4f65709 64c04bd2 43c5dba2 1c64e4f4 2e886d3c 1086c7cf ba24666e 47153f6e e6a71cef 19fac7fb 4f978cb0 0f1d002c 6f6a52e4 "
		"89d31aa4 aaec7aa4 d0b49fb6 255c04d1 3e304783 171c04b6 ea228ca1 ec10b293 f01f9bf2 1ff8d417 6e0986f4 bd596b79 9a427c0d 4f8fef58 "
		"1db30a06 4f2dad85 78d21aec 5947c8f9 abb16340 d1f7a0cb 3fe1f5ba ea627f73 86faea7c 6669f6a6 755fa82b d4bdf750 d17bfb9c f3f72753 "
		"51042248 2ee71d5c b6e289ee 67c8b7c3 6d3e088c 6d1d20e3 ea8448f0 1d1adfcf 968fd252 8d0fce0c ac0761ce 0622238a 0ab9119c 404e5f50 "
		"7ca7627d 9057daa2 f722209d 2463fee8 200c4291 3b6b6f9d 390e92aa 82d8f573 d3170250 eb0a82d3 950ad2db b90e2cd9 9dab4abd dbcd7be8 "
		"31aacdd3 54f15f12 1e91afed 96f33270 b6f8de30 d733803f ea7446ce ce9788a4 f8284128 ac67b161 d9a75ea2 8edb1132 01410d46 c36e5c70 "
		"f1d837ba e70eeaf8 2ce47b72 24add4f2 16681b4f 0d50f311 f729a144 bcc50df5 8c0adfb2 57d1fb0a 85bf21a5 56ebfafb 54e9106f 9c29ecd8 "
		"9689b8c6 cc5a8349 269a869e 1eb53813 be365482 f78276a0 d467b309 29e554a9 a5c33a73 7e4d8a77 9b02e94d 7a5d7f88 d38711f6 90168c6b "
		"0fba1b78 9e95617e c1bb4b7c 6f715786 1d2009d9 86176053 49f9d15e 90696d13 d47d0cf3 8dda47ea 3e7153e9 fe4c28a5 6fc8d604 f940d997 "
		"5da9c5f0 fb24e95b 6620e91f d4f317d3 5a530aa3 9c198371 96d0f736 6af8c1f4 9a30c72b 4fd344eb d6703c3f 31787484 ff8d560f d029c705 "
		"36c5879c e1d52539 10688586 6ab47b63 c39427b3 537ddd0d 5b4302bc 82d76a3e 64b5267d f6478818 124b6270 1df692f1 4f5dd3dc 2495fa63 "
		"d99fb2ba 1ec8b7b7 82684a88 347495ee ab041956 92318431 ff97c814 1ebc21ec 88f81830 06f4d097 e024c91b acaae59d 67684e15 32936205 "
		"82924872 e1e4bd60 f413f4a4 0e23f83d 61318799 b1d84acc 5f36c5a4 15cba328 33876922 540f0009 12126042 ee42f3e1 ed2b6ace 63945a66 "
		"6909741d 9277b4ab c0716937 cabb16f9 b292b513 12b7270f 852665a6 c87e050c bc0e6b69 958898a1 3eb1a009 5205b75e 3d1d3576 52556b90 "
		"2eebeb9b 28724d3b 0a1315b6 b4d1e63a 2dddd7e2 c5f95878 df723b84 15a4a068 2ed8d747 74eb7d2c 3bf46a32 1eaaafe2 69f06caf a4691c1d "
		"fe98a36c 46524e0b 95ec23b7 e4256f7a bc863ff9 2f71a4b0 28380243 2f8fdd7d ba28276f b43b16ab faaa9a6b cd90027f f04f248e e92d65a4 "
		"1f616398 73fda127 35cf9334 fb389067 e8f65e9d b5e3e1e6 518ff00d 6b880935 2ca17e37 398e437c 46bfa635 93f0c31f 5e964dd6 7ffef69a "
		"84d1c6ed 68efed74 e24d52ca 08c4010c 36e981d9 81e0b388 82681418 8724929c 04b8369c 32c541a3 c2acf014 d53feff1 58becd95 2b75f9ee "
		"140d6d17 436278ef e5173a56 97467250 f6fbc3f6 d4325c5b cd53fdf2 543ab765 1234c628 21b878a4 0dbed80f d71b8e43 97a2aad3 977b03bb "
		"26df2504 eea5fc86 ce3a3314 d2748f01 c2e159cb 5dbb5480 78c15bcc 57c8a783 841a8f1f 8e8f1216 0693ca2e 754aba6f d1daff6b 9f4cf093 "
		"8360b695 da9115d6 32bc1e1b bbfd08b0 c7338095 40d9adbb 5e5dc11a 23a0f23d 319d35fd 43defad5 6eb7738a 173db6af 7148ef8f b0bc9f5d "
		"6605cab9 9790dbf7 82fbc7ca 6ea0daef 76e8f064 617b5784 44d1facc cc86835b 8ff7329e 83a3e27d 85759000 38b2721e bf88baf6 482cb6f4 "
		"71dcfa6a 3a7a2338 57542a04 ba90e735 014a71b2 b181e43f 79bf049b 37aebc5d 428e816c 92088fdc 6cc0f7b4 4ae8384a 461808f7 5ae5c3c5 "
		"c4cb5d5b 8ad9179d d1562b35 3066399f fbcf1dc7 3bae3eb4 97da890c 4af09a92 16fe6918 25118867 67e6ea38 a73c34af e059434a 1897261a "
		"861f0760 5a2b19fe 12dba90f 1f744d45 6bbe7a6d 23cd52b8 335b4368 cc3c7bf7 8d0deabf f7bde186 9b056296 ce40a0b4 7d304932 4a574e1a "
		"45d99b44 cf874da3 2db84e6a 83e5f5ef 60760088 7b3d0c15 7aa60315 c0208806 d335ec93 376f779e f41d9c50 37e38e65 c1ff09cc 6f005d04 "
		"df973429 689c8160 f1a6a474 eeb91b13 e2893e08 d367f71b 6031bb32 3547040b 029728eb 7cf8e0aa 2769e220 ea526794 c8c67fa0 5bb6991d "
		"7e827859 fef0de6a d9b4ccd3 e7129c34*3 51c24c5d*2");
}

TEST_CASE("French translation: The Items screen uses, swaps and scrolls items, as its program does", "[Content][Menu]")
{
	// An Aquasol used on Fei, then on Citan until he is full (the buzzer); a swap; the pages scrolled by R1 and L1 and
	// past their ends; items that cannot be used here; a mark taken off.
	FMenuScenario Scenario{"items_use", EMenuMode::Normal, 0,
		{{0, 45}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Cross, 2}, {0, 60}, {Cross, 2}, {0, 8}, {Cross, 2}, {0, 20}, {Cross, 2}, {0, 10}, {Down, 2},
			{0, 6}, {Cross, 2}, {0, 10}, {Cross, 2}, {0, 10}, {Cross, 2}, {0, 10}, {Circle, 2}, {0, 20}, {Right, 2}, {0, 6}, {Cross, 2}, {0, 6}, {Down, 2},
			{0, 6}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 10}, {Left, 2}, {0, 6}, {Up, 2}, {0, 6}, {Cross, 2}, {0, 6}, {Cross, 2}, {0, 12}, {Right, 2}, {0, 6},
			{Cross, 2}, {0, 6}, {Cross, 2}, {0, 12}, {R1, 2}, {0, 10}, {L1, 2}, {0, 10}, {Down, 2}, {0, 5}, {Down, 2}, {0, 5}, {Down, 2}, {0, 5}, {Down, 2},
			{0, 5}, {Down, 2}, {0, 5}, {Down, 2}, {0, 5}, {Down, 2}, {0, 5}, {Down, 2}, {0, 5}, {Down, 2}, {0, 5}, {Right, 2}, {0, 6}, {Right, 2}, {0, 6},
			{Cross, 2}, {0, 6}, {Circle, 2}, {0, 8}, {Up, 2}, {0, 5}, {Up, 2}, {0, 5}, {Up, 2}, {0, 5}, {Up, 2}, {0, 5}, {Left, 2}, {0, 6}, {Circle, 2},
			{0, 60}, {Circle, 2}, {0, 40}},
		760};
	Scenario.Pokes = {
		{0x1f90, {0x5, 0x3, 0x2, 0x1, 0xc, 0x1, 0x1, 0x2, 0x1, 0x1, 0x3, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x2, 0x1, 0x1, 0x78}},
		{0x2026, {0x1, 0x6, 0xa, 0x32, 0x39, 0x33, 0x2, 0x3, 0x4, 0x7, 0xc, 0xd, 0xf, 0x13, 0x15, 0x19, 0x23, 0x2c, 0x3a, 0x5, 0x9, 0x8}},
		{0x2b8, {0xa, 0x0}},
		{0x400, {0x64, 0x0}},
		{0x404, {0x5, 0x0}},
	};
	CheckFrenchMenuScenario(Scenario, "e76f3519",
		"3f4c6d55 ef6008a9 2f8131f1*2 8da80b4e*2 54563e20*2 58e78bbf*2 9025e274*2 ae8873d3*2 c7bd5065*2 726a1769 6f375849 6236b68f 737f26fb "
		"d01c49dc eca20eea eb00e880 79da1dc0 71ec1c4f 4e816724 58743d9d 207fe77f b9fa4423 2086ff5f c2fedc78 5dfca847 2c896eef 71da1ebf "
		"0420d17f a81d30bd 00348092 90366c45 c2e23da8 c8f05725*7 b85451f2 f4f0667d*5 83da6fb2 b7172ff4*5 256e7e4f ae58ee1c 1b68f494 "
		"7edeb23f 12d49f10 f0cdbc18 4c262c15 d120ecc2 de49bccd e5ce8edd 4fba83ac aa7a3795 34cb2b4f eee46029 b05008c4 b4e6778f 5dec76b8 "
		"ba775220 f0764753 b852b642 8ef075d6 aae80621 fd57981e 1eb8807c 42e41385 fe440edf 602cd06e e37c0468 7ee8e984 6e278bad a49f05b1 "
		"aefea0e1 fc8a66ad 725d525a 694a21ca 4a508d1f*6 b1a5bc28*6 dd333918*6 a0f0b379*6 61a93816*3 02fb591a 9d6bb719 e03d82fe c65bba56*6 "
		"da3b93d1 b2f6334b 177e2f90 47869a30 4a63db6c*19 3d82c425 3f19bd2a 05437664 d14d365a*9 7606436e d14d365a 41672476*6 c22eae27 "
		"0efc667d e53a8c97 e3e03686*5 276cd24d*4 41f1a8ac c9c2f5f1 501a8345 824782a9*9 3ca91964 edd2852b 501a8345 824782a9*9 d425d44f "
		"347213b2 00835182 638fcea9*3 db2bd19e*6 8ec1540c*6 6cf76c60*4 fa08f803 6cf76c60 d37de4ce 1dc8e9f5*5 cf72591f 950c0afe ed33dbe7*4 "
		"8a97b9da*2 99dd851f 8a97b9da e8f6a7d3 021fa7eb e7082a77*4 73596658 e7082a77 b674212c 4ab6ef5c*5 8cb058c7 2bbb5b44 672b0306 "
		"486656ec*3 e28ef05a*6 2b79cb0f 057d8b72 f44c3cc9 d44fc3ca*3 41fc6d84*2 f723f20c 41fc6d84 1041b4ef d89b6824 46a9e084*4 8759de36 "
		"46a9e084 3ebf922e*6 9875b785 ab3b08db 0ae88873*4 7c431c52*6 d3ca5e8c*2 20964164 d3ca5e8c 6459b0d4 3f4d68f5 ed7abe94*4 26ad99ca "
		"ed7abe94 08da614c*6 de678de0 39fadb53 272a2f5b*4 731c6202*6 3f4d68f5*2 66b0be4b 3f4d68f5 7e31a9e3 4f412120 c2850f0a*6 a92fc918*2 "
		"087a61e8 a92fc918 9a4dd28c 7f2f619f 272a2f5b*2 c168100d*4 ea7c1c60*2 b795954b ea7c1c60 3e90a693 7228519b f2cc7d4b*3 44c3a81a "
		"f2cc7d4b 627d4f45 1951cbe9*4 ad4bd2f9 1951cbe9 8dc9fb7e 3f190f07*4 0e411dd1 4b87c630 69ee3691 aca5ecaa*4 e5a122b3 f0f00648 "
		"a94630ba 62b6b748*3 bcaca93c 7fe7521c bcaca93c 22245679 a0443ac8*2 7c841150*2 eb6d6529 7c841150 3aecc358 094948da 29817da7*3 "
		"aa336a81 29817da7 6ed517c7 dfb3a758*2 48a1ff54*2 fd39250b 48a1ff54 c73db776 ceb18c71*4 c6823905 e64de4f8 ecb284e4 79bcfa74*4 "
		"53dc1892 9a215d55 53dc1892 1da19cfb 53dc1892*2 780a2fbb*3 4585d6b4 780a2fbb 75d7d6c5 0d5dbe05*5 f1a786da 79a37b3d 9a00dc30*5 "
		"79bcfa74*3 3751423f 79bcfa74 d1117f5a 36a0379c*4 5338f332 36a0379c 0dc4f2d9 2d6bf9ef*4 fd28b0cf a28e7cb0 fc6d0e17 63df8c0a*4 "
		"1ce015b5 9d9154bf 147c7dd9 35d96015*3 4b1a1ccd 488125ca 4b1a1ccd 463cd062 42f51fd3*2 03915baf*3 a7f2510d 9e9f9808 8f6a58fe "
		"106879ba df808f6e 4101bdeb bfe6d2d8 4044c68f e52843e7 819b33dd 11d57817 c9921dd8 0afcc784 76c89311 1607c702 f5dea2e9 f314b55d "
		"d7eea588 7c730ec3 99eafd7e b0b72425 e22a595f 2f527eee f311f23f*18 91dad440*21 53553a74 4609419e 5218815c 9fe2606b 48c2f366 "
		"4eac4694 c32b986d 075eb240 6ace22dc 17716000 5a133939 959dd80f 965c96ed ce11d9ff 25491580 651be5fd 00d8438c 31c15b5b a1bfc6b1 "
		"b8e42fc1 bf27c583 e362e9a0 4a24a6c1 f696fb5a*2 24ddaa35*2");
}

TEST_CASE("French translation: Member Change swaps members between the party and the reserve, as its program does", "[Content][Menu]")
{
	// The party column's rows and wrap, the reserve's clamp, Rico picked and swapped with Citan, a pick dropped with
	// Cancel, Fei refused against Emeralda (locked) then swapped with Citan, the empty slot refused, out.
	FMenuScenario Scenario{"member_change", EMenuMode::MemberChange, 0,
		{{0, 20}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Up, 2}, {0, 4}, {Right, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4},
			{Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Up, 2}, {0, 4}, {Up, 2}, {0, 4}, {Cross, 2},
			{0, 6}, {Down, 2}, {0, 4}, {Cross, 2}, {0, 12}, {Cross, 2}, {0, 6}, {Circle, 2}, {0, 6}, {Left, 2}, {0, 4}, {Cross, 2}, {0, 6}, {Down, 2}, {0, 4},
			{Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Cross, 2}, {0, 6}, {Up, 2}, {0, 4}, {Cross, 2}, {0, 12}, {Left, 2}, {0, 4}, {Down, 2},
			{0, 4}, {Down, 2}, {0, 4}, {Cross, 2}, {0, 6}, {Cross, 2}, {0, 6}, {Circle, 2}, {0, 6}, {Circle, 2}, {0, 40}},
		500};
	Scenario.Pokes = {
		{0x1d30, {0xff, 0x7, 0xff, 0x7}},
		{0x2318, {0x40, 0x0}},
	};
	CheckFrenchMenuScenario(Scenario, "ccce104a",
		"5962a8eb 76df94bb 2add275c 06258768 e0895cb8 5edc5e1f 182c6704*2 c1a9f5b4 8385ba98 768ed397 9e3fcaf2 a5134f34 b465c967 a31b6e6f*8 "
		"23b02aad 3b449275*5 7fec1861 349af6d2*5 72a260fe a31b6e6f*5 23b02aad 349af6d2*5 72a260fe e6a01ca9*5 a4706be1 6ef978ad*5 4fcd1625 "
		"1e731061*5 1b173937 5c224245*5 0a8783f8 abcb41dc*5 af702c4c 03772fb6*5 9b792177 03772fb6*5 9b792177 03772fb6*5 9b792177 abcb41dc*5 "
		"af702c4c 5c224245*5 6fdc5b43 9a9a294e*7 70a8c885 1334db0f*5 dd2ad97e af0c10e4*4 512a6916*9 1dc65fc6 136b71c8*7 8b18251b 31df0e51*7 "
		"1308ce57 c0e23b44*5 d0d82cb2 36410565*7 7797b183 1110633b*5 508656a4 c52795a4*5 d11d2b03 fe606b3f*5 2ee6c7e9 a224f293*5 3368365d "
		"a224f293*7 414741b7 fe606b3f*5 66e60f21 228e81bb*4 6b48287f*9 e39df7ac cde9aa3f*5 573f78e3 3c3c3526*5 0f0ec9ce 49f7c650*5 6d0188c8 "
		"66211192*7 0a7458a9 66211192*7 97d4c32b ce68e800*7 d13a50e3 97ff5a75*2 24ddaa35*3");
}

TEST_CASE("French translation: The Normal Menu opens, reveals its commands and slides the party's panels in, as its program does", "[Content][Menu]")
{
	CheckFrenchMenuScenario({"normal_idle", EMenuMode::Normal, 0, {}, 100}, "b6a4e418",
		"3f4c6d55 ef6008a9 2f8131f1*2 8da80b4e*2 54563e20*2 58e78bbf*2 9025e274*2 ae8873d3*2 c7bd5065*2 077e39fc 3d6efaa7 daeb231a 10d28ea6 "
		"74e5ef9e 10ed15c4 d1b548d8 d65873be 3dc9787a 03301830 57a99e73 a8f1723f 4856fbae 629c8219 9846603d 8b7a78d6 e076bfa4 168e1db0 "
		"1d76dc8f bfeb454d 6a3a4cb4 7cb00b62 80320c78 e307975f*21 5f1ea30b*40");
}

TEST_CASE("French translation: The Status screen shows each member's sheet, stats and equipment and moves its inner arc, as its program does", "[Content][Menu]")
{
	FMenuScenario Scenario{"normal_status", EMenuMode::Normal, 0,
		{{0, 45}, {Cross, 2}, {0, 120}, {R1, 2}, {0, 60}, {L1, 2}, {0, 40}, {Down, 2}, {0, 20}, {Up, 2}, {0, 20}, {Up, 2}, {0, 20}, {Circle, 2}, {0, 100},
			{Circle, 2}, {0, 40}},
		520};
	CheckFrenchMenuScenario(Scenario, "dfe1f6fc",
		"3f4c6d55 ef6008a9 2f8131f1*2 8da80b4e*2 54563e20*2 58e78bbf*2 9025e274*2 ae8873d3*2 c7bd5065*2 077e39fc 3d6efaa7 daeb231a 10d28ea6 "
		"74e5ef9e 10ed15c4 d1b548d8 d65873be 3dc9787a 03301830 57a99e73 a8f1723f 4856fbae 629c8219 9846603d 8b7a78d6 e076bfa4 168e1db0 "
		"1d76dc8f bfeb454d 6a3a4cb4 7cb00b62 80320c78 e307975f*7 15fac373 3303fc38 b5e8e8ee f94e5120 fe19596a 1a6af0bd d8855f08 42d96700 "
		"5f0eacfe 96b5da70 7b8e0993 0e5c9ab9 c6e04648 01861972 f121e839 3f6625b9 49645981 e1709111 98509aab 31d55008 63f623aa 71418c7f "
		"e68cc794 81701477 27592c6d 6a6fcd90 25b9fdc9 ae537541 01bd674f e0f943af c4cfd8f3 d64bbf43 fa141d0d 6b658684 cd4ff320 c258dd53 "
		"2b00960a 3cac34d8 acdda991*36 7c398721*48 30ed7886 7c398721 0d80abe4 2d7f1791 9223ca52 5d904297*7 79812a01*50 12e229ca 79812a01 "
		"5f7403fe b9f00785 46ab91a5 a315dd70*5 329a5f28*32 24e2ce6e 329a5f28 df585f0c*20 f9fbacd0 df585f0c 329a5f28*4 d23f5dc5*16 a45160fe "
		"d23f5dc5 f47377c7*20 2f844154 6b4208ee af7d71c9 f5ac7cdd 2a3ae7b9 ede9735b 6d53a9d7 5545dba7 d7233f9a 546dae69 4071ca45 552c50ed "
		"5aa17486 02c84efe 2f302222 55afb7d1 8a8fb5e0 5e80d3f0 439bb624 077a1696 17cfa53e af4c32cb 9d6a803c 77966f9a*59 d8737e79*20 "
		"fcb5bf1b 9f462f02 8086a002 e2f92f50 fdb51b2a fd791101 c8ebaa4f 837c4f06 d158a831 c36ac724 f04d09bf f8d5ec24 72ea1723 623b7228 "
		"cd07ac6e 9547c472 a4e8c4bb 9ae7ac54 0f83836b 6af50c25 f78a851d 222e48f1 56ee938b dfbe9524*2 24ddaa35*3");
}

TEST_CASE("French translation: The Skills screen lists each member's Deathblows and the Status screen comes back after it, as its program does", "[Content][Menu]")
{
	FMenuScenario Scenario{"normal_skills", EMenuMode::Normal, 0,
		{{0, 45}, {Cross, 2}, {0, 120}, {Cross, 2}, {0, 40}, {R1, 2}, {0, 40}, {L1, 2}, {0, 30}, {Circle, 2}, {0, 40}, {Circle, 2}, {0, 100}, {Circle, 2},
			{0, 40}},
		600};
	CheckFrenchMenuScenario(Scenario, "0d135fb5",
		"3f4c6d55 ef6008a9 2f8131f1*2 8da80b4e*2 54563e20*2 58e78bbf*2 9025e274*2 ae8873d3*2 c7bd5065*2 077e39fc 3d6efaa7 daeb231a 10d28ea6 "
		"74e5ef9e 10ed15c4 d1b548d8 d65873be 3dc9787a 03301830 57a99e73 a8f1723f 4856fbae 629c8219 9846603d 8b7a78d6 e076bfa4 168e1db0 "
		"1d76dc8f bfeb454d 6a3a4cb4 7cb00b62 80320c78 e307975f*7 15fac373 3303fc38 b5e8e8ee f94e5120 fe19596a 1a6af0bd d8855f08 42d96700 "
		"5f0eacfe 96b5da70 7b8e0993 0e5c9ab9 c6e04648 01861972 f121e839 3f6625b9 49645981 e1709111 98509aab 31d55008 63f623aa 71418c7f "
		"e68cc794 81701477 27592c6d 6a6fcd90 25b9fdc9 ae537541 01bd674f e0f943af c4cfd8f3 d64bbf43 fa141d0d 6b658684 cd4ff320 c258dd53 "
		"2b00960a 3cac34d8 acdda991*36 7c398721*48 48c7d705 d151a89a 11b56261*40 673d2133 11b56261 b24335de*40 9f005e3e b24335de "
		"11b56261*30 d832cebd 608f1798 80f50642 f56d59ed*39 3f8a2084 faea8c10 11d1ff38 eb6ec731 da46126f bf95c90d 569573dc 8bfcff8b "
		"103324c6 156f203e 4be0a5b0 ff6ae822 a192c53e f898b613 de1a16ab 70ecd9a1 06b613f9 49b55d81 c0769557 1b39d76c 970e76d8 74d50a1a "
		"8eed901b dd2f5934*11 77966f9a*60 d8737e79*8 fcb5bf1b 9f462f02 8086a002 e2f92f50 fdb51b2a fd791101 c8ebaa4f 837c4f06 d158a831 "
		"c36ac724 f04d09bf f8d5ec24 72ea1723 623b7228 cd07ac6e 9547c472 a4e8c4bb 9ae7ac54 0f83836b 6af50c25 f78a851d 222e48f1 56ee938b "
		"dfbe9524*2 24ddaa35*3");
}

TEST_CASE("French translation: The Skills screen shows learned Deathblows with their combos and the others' learning, as its program does", "[Content][Menu]")
{
	// Fei has learned his first three Deathblows, Citan his first and fifth; the upper tier is open; both have counters under way.
	const auto Counters = [](uint32 Step, uint32 Modulus)
	{
		std::vector<uint8> Bytes;
		for (uint32 Counter = 1; Counter <= 7; ++Counter)
		{
			const uint32 Value = Counter * Step % Modulus;
			Bytes.push_back(static_cast<uint8>(Value));
			Bytes.push_back(static_cast<uint8>(Value >> 8));
		}
		return Bytes;
	};
	FMenuScenario Scenario{"normal_deathblows", EMenuMode::Normal, 0, {{0, 45}, {Cross, 2}, {0, 120}, {Cross, 2}, {0, 40}, {R1, 2}, {0, 40}, {Circle, 2}, {0, 30}}, 330};
	Scenario.Pokes = {{0x16C0, {0x00, 0xE0}}, {0x22B6, {0x00, 0x40}}, {0x26C + 0x90, Counters(37, 400)}, {0x26C + 2 * 0xA4 + 0x90, Counters(91, 700)},
		{0x16C0 + 2 * 0x20, {0x00, 0x88}}};
	CheckFrenchMenuScenario(Scenario, "80c35f25",
		"3f4c6d55 ef6008a9 2f8131f1*2 8da80b4e*2 54563e20*2 58e78bbf*2 9025e274*2 ae8873d3*2 c7bd5065*2 077e39fc 3d6efaa7 daeb231a 10d28ea6 "
		"74e5ef9e 10ed15c4 d1b548d8 d65873be 3dc9787a 03301830 57a99e73 a8f1723f 4856fbae 629c8219 9846603d 8b7a78d6 e076bfa4 168e1db0 "
		"1d76dc8f bfeb454d 6a3a4cb4 7cb00b62 80320c78 e307975f*7 15fac373 3303fc38 b5e8e8ee f94e5120 fe19596a 1a6af0bd d8855f08 42d96700 "
		"5f0eacfe 96b5da70 7b8e0993 0e5c9ab9 c6e04648 01861972 f121e839 3f6625b9 49645981 e1709111 98509aab 31d55008 63f623aa 71418c7f "
		"e68cc794 81701477 27592c6d 6a6fcd90 25b9fdc9 ae537541 01bd674f e0f943af c4cfd8f3 d64bbf43 fa141d0d 6b658684 cd4ff320 c258dd53 "
		"2b00960a 3cac34d8 acdda991*36 7c398721*48 48c7d705 d151a89a 4b5fdf9b c7f062f4*39 46bc8c25 c7f062f4 0a022c9c 7f9288fd*39 050beb11 "
		"608f1798 80f50642 f56d59ed*75");
}

TEST_CASE("French translation: A shop sells weapons and accessories into a cart and takes the gold on Yes, as its program does", "[Content][Menu]")
{
	// Shop 3: who can use the goods and what they change, units put in the cart and one taken out, the list scrolled,
	// the total accepted with Yes.
	FMenuScenario Scenario{"shop_buy", EMenuMode::Shop, 3,
		{{0, 20}, {Cross, 2}, {0, 70}, {Right, 2}, {0, 6}, {Right, 2}, {0, 6}, {Left, 2}, {0, 6}, {Down, 2}, {0, 6}, {Down, 2}, {0, 6}, {Down, 2}, {0, 6},
			{Down, 2}, {0, 6}, {Down, 2}, {0, 6}, {Down, 2}, {0, 6}, {Down, 2}, {0, 6}, {Down, 2}, {0, 6}, {Right, 2}, {0, 6}, {Up, 2}, {0, 6}, {Up, 2},
			{0, 6}, {Up, 2}, {0, 6}, {Cross, 2}, {0, 30}, {Left, 2}, {0, 6}, {Cross, 2}, {0, 40}, {Circle, 2}, {0, 60}},
		700};
	Scenario.Pokes = {
		{0x1d30, {0x1f, 0x0, 0xff, 0x7}},
		{0x1924, {0xb8, 0xb, 0x0, 0x0}},
		{0x1d38, {0x3}},
		{0x1d9c, {0x1}},
	};
	CheckFrenchMenuScenario(Scenario, "921e8f63",
		"1b6d91ec ef6008a9 14c42477*2 ff9aeb5c*2 f42b8755*2 4c878366 8a1d0b75*13 59e34ee9 00726904 8f57c252 4e53035d 908a28a1 e19c0c0a "
		"421f5b48 b9e80b1d cb1c2f88 8156c4ac f4525203 f579df1f 560ec78f 080c90a1 20d7073d 781eaa72 d4157539 f6608731 90b4d77c 2fd4c93a "
		"c1d432ba 97ef7c20 e8a7bde4 d8fd0f6d e8213fdf bae46813 66c49c9d 4046f370 3de46c80 2682c203 aba33946 397c868e 9dafb7b8 3e77d384 "
		"10c48170 fc0cb2af*5 42b857a8*6 59acce92*6 6bd9b8ac*6 d6540769*6 fc0cb2af*6 42b857a8 a0d7ab48 42b857a8 33caa68c 75608930*2 "
		"825e02e3*3 8984145c 825e02e3 8df2920e 259eed77*5 f01293c9 c26be8c9 eeec8e0f 1f58993b*4 34f73123 214cec4c 34f73123 6708488a "
		"f0299d05*2 96ce0647*3 4162d978 96ce0647 e53ecb74 84231747*5 9604af3b 397d03b3 946dbd4f fb72f367*4 4202affc 9f880db5 4202affc "
		"c5e2a7b3 78ec0e4f*2 ed7630ef*3 653f65c1 ed7630ef cbd51bb5 5717e458*5 0d10a1bd e10e1a15 2139d7bb efe80085*4 6de6a6f7 3fe905ae "
		"6de6a6f7 1b959b59 f1a988dd*2 bca69d78*3 43b68b4c bca69d78 4b41ec48 13dcd8b5*5 54abb3cc 00ea6c19 6ec3e879 ff6875b1*4 fbffdeae "
		"2787fc87 fbffdeae 7aeca478 724c5c51*2 9c831c53*3 ef68ee0c 9c831c53 e1203a96 99d72812*5 9a082588 ce0cfaee 707cd205 f4c14bc0*4 "
		"5a8c0108*2 fd0536bd eb389c92 2968e306 62e514d3 4ca8fb7b c7d4adb1 119189de b268bed2 e5bf4080*23 2091890e 3d00db84*8 59e34ee9 "
		"d7eb9bf9 aa0efb55 a0cbb71c*38 31e51ae3 a0cbb71c*2 24ddaa35*2");
}

TEST_CASE("French translation: The Abilities screen opened from the Status screen returns to the sheet, as its program does", "[Content][Menu]")
{
	// From the Status screen's inner arc: no flight of its own; an Ether used; back to the sheet.
	FMenuScenario Scenario{"status_abilities", EMenuMode::Normal, 0,
		{{0, 45}, {Cross, 2}, {0, 120}, {Down, 2}, {0, 8}, {Cross, 2}, {0, 20}, {Right, 2}, {0, 6}, {Cross, 2}, {0, 20}, {Cross, 2}, {0, 10}, {Circle, 2},
			{0, 10}, {Circle, 2}, {0, 60}, {Circle, 2}, {0, 60}, {Circle, 2}, {0, 40}},
		420};
	Scenario.Pokes = {
		{0x16c2, {0x0, 0xc0}},
		{0x2b8, {0x14, 0x0}},
	};
	CheckFrenchMenuScenario(Scenario, "b5177900",
		"3f4c6d55 ef6008a9 2f8131f1*2 8da80b4e*2 54563e20*2 58e78bbf*2 9025e274*2 ae8873d3*2 c7bd5065*2 091c049f 440bd27c 56d6107a 7705d96b "
		"a29cdba6 f9273f7a 75c8e878 e15ad4c1 84777e78 062cac18 639c5411 c7c775cc b5925ef2 9e6bede5 60127e59 611b78d1 4f3023d8 81313014 "
		"3b4fc1f3 f640c506 ed2122aa 86bbeba5 203fc213 30714ccb*7 b1ebdbed ee8ab479 fd79929f e0b25a5a 8fbbb6d0 ac362249 b43a21f4 85ecef2d "
		"b4221d59 a40a81a0 0f3d8748 d2f305a1 961afb7a 9cd4eb9f 50915de6 788c4387 0dd48a5e 128ac236 7482cf51 0b87a5f1 7296afdb d77bdc45 "
		"6b1af138 f6a00800 3922b767 a55e161b 053e7788 d986c142 6e4a3144 4ba9ae7d 3a1da346 9045b377 339fc473 1df93473 cb21b34d 5fea9053 "
		"a647cc87 ad5617d0 cce86bdb*36 caa5c051*48 da8ea16c caa5c051 f2740165*8 5f9eb1bc 0fcef025 2dc73c85 d95967cd*4 c9b6a552*6 9f6b853d*6 "
		"37b60a95*3 2119a296 37b60a95 e31543a7 0fdd8578*5 e4064d00 dd1af7ec a8abe05d 24e6875c*19 fad733eb e65e70a0 ef45a4e1 7824b55c*9 "
		"6dbef835 2c94294f 1e9b6274 b9af3e6b 8e07ec41*4 03ad7f2f*4 e592aa24 26e709fa 130461fc f03216aa*2 10d07bdf f03216aa*40 4faf0cba*16 "
		"25d18cd3 b20a8bb4 97926b95 d68b80e1 c7c4c9b6 1fbd7c15 543ae842 beff2b2b 0f5ee113 b363c04a b6dc68f7 a5a21dce 30f7a2b2 9897902f "
		"7a2eecb9 1059aa53 45f9ead4 5ff7bb7a 0c955a14 49ae9790 dbe94214 12f0ea20 375ebb3c e65ca676*21 114dff53*18 a4f39891 1b43b108 "
		"96f01dc2 5696fd7e 15948309 eb0b0056 347e8e14 fe811c70 64d7958d 277702a2 7790ac54 23d9d7cd c6f2bcd2 0f6e1407 a682bbd9 56aee288 "
		"3712ba0e 27c9cf6e 332a7c56 cf663c9d aa1d4f11 13c8aec8 c235d98a dfbe9524*2 24ddaa35*3");
}

TEST_CASE("French translation: The disc change goes on to disc 2 when saving is declined, as its program does", "[Content][Menu]")
{
	// "Save?" answered on its left option, then "Are you sure you don't need to save?" with Yes.
	FMenuScenario Scenario{"disc_change_skip", EMenuMode::DiscChange, 0,
		{{0, 50}, {Left, 2}, {0, 6}, {Cross, 2}, {0, 30}, {Right, 2}, {0, 4}, {Left, 2}, {0, 6}, {Cross, 2}, {0, 60}},
		200};
	Scenario.Disc = 2;
	CheckFrenchMenuScenario(Scenario, "d4621eaa",
		"478cd0c6 09a4e140*33 a355032a 116d7d6e 2f1dcc45 eaf6f244 3659b4fa f20f6c1d bec88b2b 5c970eff e0ccd49d*9 965e8812 a9c8be2c*7 "
		"2557d1a0 09a4e140 a355032a 116d7d6e 2f1dcc45 eaf6f244 3659b4fa f20f6c1d e07ab275 adb0d24f 09e97f92*22 04d3f3fb 09e97f92*5 04d3f3fb "
		"d2bba3c5*7 ad0b5748 09a4e140 f5dc5faf 09a4e140*35 24ddaa35*3");
}

TEST_CASE("French translation: The disc change's Save? cancelled asks whether to go on without saving, as its program does", "[Content][Menu]")
{
	// Cancel on "Save?" asks the same question, answered Yes.
	FMenuScenario Scenario{"disc_change_cancel", EMenuMode::DiscChange, 0,
		{{0, 50}, {Circle, 2}, {0, 30}, {Left, 2}, {0, 6}, {Cross, 2}, {0, 60}},
		160};
	Scenario.Disc = 2;
	CheckFrenchMenuScenario(Scenario, "87cdb9f6",
		"478cd0c6 09a4e140*33 a355032a 116d7d6e 2f1dcc45 eaf6f244 3659b4fa f20f6c1d bec88b2b 5c970eff e0ccd49d*9 37df7190 09a4e140 a355032a "
		"116d7d6e 2f1dcc45 eaf6f244 3659b4fa f20f6c1d e07ab275 adb0d24f 09e97f92*22 04d3f3fb d2bba3c5*7 ad0b5748 09a4e140 f5dc5faf "
		"09a4e140*35 24ddaa35*3");
}

TEST_CASE("French translation: The Equip screen tries accessories on, keeps one, puts one back and takes one off, as its program does", "[Content][Menu]")
{
	// Fei's slots, an accessory list scrolled and one chosen, a list left with Cancel; Citan's accessory taken off.
	FMenuScenario Scenario{"equip_accessories", EMenuMode::Normal, 0,
		{{0, 45}, {Down, 2}, {0, 4}, {Cross, 2}, {0, 60}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 12}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4},
			{Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Up, 2},
			{0, 4}, {Up, 2}, {0, 4}, {Cross, 2}, {0, 12}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Cross, 2}, {0, 8}, {Down, 2}, {0, 4}, {Circle, 2}, {0, 8},
			{R1, 2}, {0, 20}, {Up, 2}, {0, 4}, {Cross, 2}, {0, 8}, {Cross, 2}, {0, 12}, {L1, 2}, {0, 20}, {Up, 2}, {0, 4}, {Up, 2}, {0, 4}, {Cross, 2},
			{0, 12}, {Circle, 2}, {0, 60}, {Circle, 2}, {0, 60}, {Circle, 2}, {0, 40}},
		560};
	Scenario.Pokes = {
		{0x1e00, {0x1, 0x2, 0x3, 0x63, 0x5, 0x1, 0x1, 0x2, 0x1, 0x1, 0x1, 0x1, 0x1}},
		{0x1ec8, {0x2, 0x3, 0x11, 0x12, 0x5a, 0x5b, 0x5d, 0x5e, 0x62, 0x63, 0x68, 0x74, 0x19}},
	};
	CheckFrenchMenuScenario(Scenario, "84b783c5",
		"3f4c6d55 ef6008a9 2f8131f1*2 8da80b4e*2 54563e20*2 58e78bbf*2 9025e274*2 ae8873d3*2 c7bd5065*2 077e39fc 3d6efaa7 daeb231a 10d28ea6 "
		"74e5ef9e 10ed15c4 d1b548d8 d65873be 3dc9787a 03301830 57a99e73 a8f1723f 4856fbae 629c8219 9846603d 8b7a78d6 e076bfa4 168e1db0 "
		"1d76dc8f bfeb454d 6a3a4cb4 7cb00b62 80320c78 e307975f*7 91e69c42 7c4ac604*5 f2b58f34 899f9986 25b38d29 75969f2c ddc784c5 42ae9721 "
		"caf65766 bc10ff12 d056255b 24e3e189 bf0f5d9b b0b2b9b3 3ef3715c 8ae4297c eb66f394 33a0f237 91cd1769 a7a03482 25d81b0b fcfc95f8 "
		"3961e821 b7f4123a 82fb35ce 8314ffa7 368ba2dc d1b00c0a cfb8bca4 932ffae3 7d891965 2466c833 e42eff33 7d3286bd 02f05040 65990742 "
		"bc68f5d6 083649d6 d99d0351*26 355023fb d99d0351 ade79310 c87a212b*5 474a14bf ee8d61fe 2781fc14 e8c03e3a*4 2eee2047*6 4994f130 "
		"0a864ce6 4994f130 a4a80d82 882a4ab7*2 04526a44 d0da23c1 04526a44 91ee823c 18287bad*2 b672b993 b3cbf892 b672b993 6d075d94 "
		"92242b77*2 be69b33b 99d168b9 be69b33b e1275136 b8c05064*2 0c0fb0af bd86a297 0c0fb0af 79424291 cf3f2dde*2 66dde54f 6db043ca "
		"66dde54f ceefa0a0 43a5c4af*2 22929380 29f1456a 22929380 610ebc6d 546faba9*2 e9bc36a3 66a05051 e9bc36a3 5a9aaa65 29d322cc*2 "
		"55382e85 b3c89869 55382e85 bd6b7b7a 23794d1c*2 d397d392 d804f834 d397d392 db123825 b5a473d5*2 d4912ebe 9e95f110 d4912ebe 2df68a16 "
		"3eabfa97*2 c22f0ab6 b4c98ad9 c22f0ab6 65be4cbf 50ff878a*2 248af862 306e33d8 4f5947e9 e0a6e04c 2ad35cec*11 0c30239e 2ad35cec "
		"a2255218 95b5003c*3 392f9a2e 95b5003c 0a3fdf1f 3a513e0f*3 6c6d2a4d a732a70a 9a099dc6 757ed937 593591ac*6 2410d85c aaa42557 "
		"b9732f3c 116eac3c*3 4767255b a72641e1 c6214439 3a513e0f*7 eb86b0f3 3a513e0f 0bb401f6 06d53a21*19 28bde42a 06d53a21 616cf3c8 "
		"c8e41316*3 6258fed6 05accfbd 2414648b 6d45c4f3*2 8b23e718*5 84036f45 a5bf9965 7d676d64 6efd2416*11 e9f13fed 6efd2416 97c3c235 "
		"38d5eea0*19 37d1db08 38d5eea0 e7d9c16e d14c5a59*3 4d236bf5 d14c5a59 cf8e8a86*4 9e53a3c7 83d9dbe4 f87601f4 2ffbdb6e*3 12d1753c*6 "
		"713fc5d6*2 a8832aa6 b5f7c423 5258439b b5f7c423*59 9e82e8fb af83de45 412ea17f 55eaa211 e6935c26 5f90e88c b2f8fd0f 17822b16 3ae60066 "
		"24fd3da2 c4c9a6e5 6198821c 65b11aa1 ab8c18f5 bf79ac17 ab261766 4a6a105e 499696da c38479ac 92193bf0 d96a5653 008f50b5 5e09354e "
		"c75113c4 b4429962 6c8f9eae*33 f7631da3*4 a6fe4b29 a23eee8e c59987f2 11d0aa4d b2f14c7c a86f6071 b4162856 8f588a06 363d898a 1c2034d2 "
		"a267f2e1 3a2e92dd 3cb1478b 6d3b55e1 1a183c9e 9ff77ec1 18359da5 39cf2417 aef5b644 9be85b91 d315efd2 11369c3c 88613720 cfe89f75*2 "
		"24ddaa35*3");
}

TEST_CASE("French translation: The title's Delete and Copy change the cards and read them again, as its program does", "[Content][Menu]")
{
	// No.3 deleted (both questions on their left option); the second card's No.2 copied to the first, then again
// ("Cannot copy"); out by Cancel.
	FMenuScenario Scenario{"file_title_delete_copy", EMenuMode::LoadGame, 0,
		{{0, 40}, {Cross, 2}, {0, 70}, {Down, 2}, {0, 6},
			{Down, 2}, {0, 6}, {Cross, 2}, {0, 40}, {Down, 2}, {0, 20}, {Cross, 2}, {0, 30}, {Left, 2}, {0, 6}, {Cross, 2}, {0, 30},
			{Left, 2}, {0, 6}, {Cross, 2}, {0, 40}, {Cross, 2}, {0, 40}, {Up, 2}, {0, 6}, {Cross, 2}, {0, 40}, {Right, 2}, {0, 12},
			{Cross, 2}, {0, 30}, {Left, 2}, {0, 6}, {Cross, 2}, {0, 40}, {Cross, 2}, {0, 40}, {Cross, 2}, {0, 40}, {Right, 2}, {0, 12},
			{Cross, 2}, {0, 30}, {Left, 2}, {0, 6}, {Cross, 2}, {0, 40}, {Cross, 2}, {0, 40}, {Circle, 2}, {0, 60}},
		800};
	Scenario.Cards = {{{FirstSave(), ThirdSave()}, {LongSave()}}};
	CheckFrenchMenuScenario(Scenario, "666f5911",
		"b4ead516 ef6008a9 503a6645*2 4b77ef5f*2 db3467bc*2 261cb849 3ef16831*32 e951fa68 2c3f221c 40544a35 d9e0a11a 6c1f0eba 0812f6e4 "
		"85c33bdd 7000f648 d4899d32 fca90238 b14e88a1 a06e7914 083b57a7 af37ab8e 8ec30d28 7efbb255 7ff764fa 2ee8a242 f132b542 4849991f "
		"35946729 20a9ce58 821159d0 07d8538b 48f4cde9 b6e7a5d5 47bdca59 e02d2ac4 016df4d1 c0c73858 5a09f133 c7a7e080 56567f8b 1b2f53c2 "
		"3b3e2813 5c0ba11c a91614a4 512bef97*35 328b0f24 512bef97 4c5c9cd1*6 eadcb8b9 4c5c9cd1 34158e75*6 12b3394f 10291d86 4ca94f07 "
		"37a33db2 c63dbcd7 20357320 2035287b 205698ad 59edad1f 3cd9f09f 5f28ec23 f90ad6b0 a9faecb0 5d516a9c a403daa8 1deea885 5ca471ae "
		"a53f90cd 65fce5b4 e375abba 3fdf76e7 4c007c27 c409e332 b8b4a4ce 693c1830 7dfc3dc1 343b15ae e56b74cd e157dda2 c17a1925 0f7ee778 "
		"4f6d2d18 0f7ee778 c17a1925 e157dda2 e56b74cd 343b15ae 7dfc3dc1 693c1830 b8b4a4ce c409e332 4c007c27 e1812e3b 5b442eba 42c38c88 "
		"aa33d54e b2672e30 bcc046e6 cde5661e c1f9b787 c96f8cce 9a19fd3b c381f79f adcb4bde a9ffa63c 26f98e06 a0ed07c4 74bc4db2 81179a19 "
		"095d3ae2 fdae02b2 623e3b79 eca5e67d 0b353744 7910d858 5d48a130 5145db20 0830deb9 cc51f922 96ada054 829634ce b7b50981 a88f5125 "
		"72435710 0a759f52 701eb85e 7d97bced ae8d2bfc 7e48a064 bcfe4811 499ca7e4 09d01969 875a4dad 4572b7f3 9c64abdc 6fda27b8 547fef8e "
		"794f5d3f 617c975c fe93a3fb 2f8aad83 d9ab92bb fd4e8fb5 2a11218a 499166e2 55e51e0d 98e236ac f6d28be9 7b34fd73 fa91db0b fa86c53c "
		"ba7e288f 908881d8 c533700d 1071f8a3 4f1adb6e e229ad86 a51911b1 83a94ff3 0cdcfd11 8915d1fb a76b995f 88592d22 1083186d 6e1b87d8 "
		"98fc3b01 11f84355 41ebafba bb1693d4 ad603fb0 e4286d4d 49188cc5 a994349b 346f35d3 7db4eef1 56375cbe 143939e6 20deab62 143939e6 "
		"56375cbe 7db4eef1 346f35d3 a994349b 49188cc5 e4286d4d ad603fb0 03bed4a7 f29f6b5e 74b8b590 c5bdaf40 c0df673e 49598168 90bef421 "
		"c19d3fa7 259b30f1 aa33d54e 518ee651 e1447729 69db4a38 1c00ee37 5fe072f5 b37ccecc cb2e67c5 691f487b 58281e72 49818af8 934f5939 "
		"8a672ec3 87fc9e76 e0287e08 87fc9e76 8a672ec3 934f5939 49818af8 58281e72 691f487b c57eaab1 683bdcc0 72d739ac 71432f1f f981b965 "
		"3329b958 b0dffe24 b93f975d 29164845 0c8fbaca f0432525 72a97bd3 81b33d87 b2176bce 3f4249a0 e2e7310c 61fb6063 cb6a0b36 a68dec52 "
		"33102993 de862077 d7961a77 55318b85 ea5dbdfe*39 5599b049 ea5dbdfe 52f3f775*6 a9733cb4 b361b8f6 226c2a21 4506acba 6f1a691e 0bebd63f "
		"7c574c03 f3374fd5 3aef66ab 233a05a3 dff4269b c31a86ac 656ff365 362556c4 aa6a6800 f9107b73 aef1f17a 88d061ed 27550636 f9b934f5 "
		"27550636 88d061ed aef1f17a f9107b73 aa6a6800 362556c4 656ff365 c31a86ac dff4269b 233a05a3 3aef66ab f3374fd5 7c574c03 0bebd63f "
		"6f1a691e 4506acba 2664486a 49a7341c 27fe52b9 354d2922 7a5ec2e0 5149b4d3 2925175f 9a1eb747 17b079bd d3aa0af6 4de56bde aa8c7014 "
		"fc60c83d 49ccff76 343d177c ed629477 343d177c 49ccff76 fc60c83d aa8c7014 8fb1d8a9 5dcc4059 854e97da 86156bec 714848e9 7f199b41 "
		"59ba2a78 037fbd9c efe460da 77af7bc0 a5f37f01 0fd6f77d 3c89a1e5 3ff68580 232c60cc 5c49ad68 f3891d27 e7b8d768 236f1d44 c4b52b5f "
		"349c2ea5 ccfc866c 4449ee64 400e6a19 c5df6d7b fbd9e544 ebf4cfb3 8ffa0ecc ebf4cfb3 fbd9e544 c5df6d7b 400e6a19 cb9e2f3c d18ecc53 "
		"64cac575 9ea97edc e3d75759 2e34a346 ba18ffae c5137010 68acdafd 6df03977 5c78e650 b574106f 40ad1d3e 9f96cb85 d5bbf868 61a20768 "
		"42abbf3d 9fe4d4df 885cbca0 eecc232c e9adbb08 cf78d204 e675a585 abe2faaf c0adea7c 1431e690 668db058 311859ff 668db058 1431e690 "
		"c0adea7c abe2faaf e675a585 cf78d204 e9adbb08 eecc232c 885cbca0 9fe4d4df b762a260 2969623c 4d165f27 9a2d6943 553f7bf9 497346ce "
		"b272291a 86aa5005 b2375357 e8cf9903 594b6d3d d6230349 51e91abb 1fa7f7c9 f76ec16a 4c5c9cd1*39 8e7f8635 4a7a93c1 4b44e52d 6e09ca2a "
		"1503e96b 25acdc0e 56b85c82 be32bf7d 9b33a551 0f060570 196f65fc 79e23470 8ba38abb 1189ab58 a00aaee0 355e668e a5b38603 7eb1623e "
		"3d49a199 903fbb62 44c0adc6 87c97d5d ad77d994 2e569a41 ffecfd49 cd719133 6970641b 59950774 b7b5c943 ab915150 eda91982 e57f7b9a "
		"eda91982 ab915150 b7b5c943 59950774 6970641b cd719133 ffecfd49 2e569a41 ad77d994 87c97d5d 9d74972c 6666cea9 506101e3 de7f414f "
		"01a2801b 32212297 b7ced11f cb0e20a0 06c07afe 476d91e2 170779d4 beae6807 c417636a 9977afc6 c2ee2ad2 a8a227da 2204f71f 055a291c "
		"2f26990d be0eb563 3e3d60fa b4a4efff 97d1c99b 2e06792d ca81aeac ff00fa37 3a6d64fd 6ff99a51 7c622f00 38213e72 8bc46f2a 543ef906 "
		"8e6f8b66 d5ace022 216d4023 a7b5c84b 39b2af21 c95278c5 5b4bf035 9f86d07c 97cb21b3 7b367daf fc51933d 2520fb03 30f05fa0 f3748de1 "
		"18256dfb 18ad374b 795729e4 53d5ceb1 14d8c464 ad22e767 0bf9c9dc 9f028126 e0c5beee a1be30f5 88deb3af 890bc77c c6381a28 cb266274 "
		"b480c815 cfb89046 c0bba783 3973fb6f 035a4207 1d8bd6c7 9f8321d6 e14d331e 52cd2162 3c7ba5b4 888c2a43 cad0d534 1e316423 8b982680 "
		"8b35063f fdc7842e 1ae5034f 5991e159 6acb96a5 cf7cd8b7 29c262f3 1d5a399e 445e80d5 26389734 e94b27a6 b17bec3d e94b27a6 26389734 "
		"445e80d5 1d5a399e 29c262f3 cf7cd8b7 6acb96a5 5991e159 1ae5034f fdc7842e b9d8939b 476d91e2 f24977b8 4c5c9cd1*39 20478b5c 56d6a188 "
		"9ae8eecc*73");
}

TEST_CASE("French translation: The title's commands find no data on empty cards, as its program does", "[Content][Menu]")
{
	// Each command's notice, then out by Cancel.
	FMenuScenario Scenario{"file_title_empty", EMenuMode::LoadGame, 0,
		{{0, 40}, {Cross, 2}, {0, 70}, {Cross, 2}, {0, 40},
			{Cross, 2}, {0, 30}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 40}, {Cross, 2}, {0, 30}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 40},
			{Cross, 2}, {0, 30}, {Circle, 2}, {0, 60}},
		400};
	Scenario.Cards = {{{}, {}}};
	CheckFrenchMenuScenario(Scenario, "c902a692",
		"b4ead516 ef6008a9 503a6645*2 4b77ef5f*2 db3467bc*2 261cb849 3ef16831*32 e951fa68 2c3f221c 40544a35 d9e0a11a 6c1f0eba 0812f6e4 "
		"85c33bdd 7000f648 d4899d32 fca90238 b14e88a1 a06e7914 083b57a7 af37ab8e 8ec30d28 7efbb255 7ff764fa 2ee8a242 f132b542 4849991f "
		"35946729 20a9ce58 821159d0 07d8538b 48f4cde9 b6e7a5d5 47bdca59 e02d2ac4 016df4d1 c0c73858 5a09f133 c7a7e080 56567f8b 1b2f53c2 "
		"3b3e2813 5c0ba11c 469fe348 153cc36d*35 bced3729 b4f518d8 95fb8f92 ffd5502d 01bf7850 ba0dfd89 e4fc0067 71b08702 b3867430 "
		"d1ae5fd8*33 4f5776e5 b4f518d8 7c06d432 153cc36d*29 d799490b 153cc36d 924e5565*6 84ab76c7 5cb726c8 f86b880c 8a67d43c eef8eb4f "
		"a54ae30b 117cd650 70274f6b 380140c2 ccbe3020*33 b25bf98d 5cb726c8 83ff0ea4 924e5565*29 c7778a93 924e5565 8e5d93f8*6 7cae3e23 "
		"aaf4ccc3 1d144b3d 7bdba980 b1ebad71 17941d2c dafbbe7e e3d2da6e 94073888 1953effc*33 9f09f633 aaf4ccc3 b46e4204 8e5d93f8*29 "
		"0678742c 56d6a188 9ae8eecc*47");
}

TEST_CASE("French translation: The title's cards walk a full card and refuse a copy to it, as its program does", "[Content][Menu]")
{
	// The full card's grid walked, a Load answered No, No.1 copied to the full card (no room).
	FMenuScenario Scenario{"file_title_full", EMenuMode::LoadGame, 0,
		{{0, 40}, {Cross, 2}, {0, 70}, {Cross, 2}, {0, 40},
			{Right, 2}, {0, 8}, {Right, 2}, {0, 8}, {Right, 2}, {0, 8}, {Right, 2}, {0, 8}, {Right, 2}, {0, 8}, {Right, 2}, {0, 8},
			{Down, 2}, {0, 8}, {Down, 2}, {0, 8}, {Down, 2}, {0, 8}, {Down, 2}, {0, 8}, {Up, 2}, {0, 8}, {Left, 2}, {0, 8}, {Left, 2},
			{0, 8}, {Left, 2}, {0, 8}, {Cross, 2}, {0, 30}, {Cross, 2}, {0, 30}, {Circle, 2}, {0, 30}, {Down, 2}, {0, 6}, {Cross, 2},
			{0, 40}, {Cross, 2}, {0, 30}, {Left, 2}, {0, 6}, {Cross, 2}, {0, 40}, {Cross, 2}, {0, 40}, {Circle, 2}, {0, 60}},
		640};
	Scenario.Cards = {{{FirstSave()}, FullCard()}};
	CheckFrenchMenuScenario(Scenario, "17f808e7",
		"b4ead516 ef6008a9 503a6645*2 4b77ef5f*2 db3467bc*2 261cb849 3ef16831*32 e951fa68 2c3f221c 40544a35 d9e0a11a 6c1f0eba 0812f6e4 "
		"85c33bdd 7000f648 d4899d32 fca90238 b14e88a1 a06e7914 083b57a7 af37ab8e 8ec30d28 7efbb255 7ff764fa 2ee8a242 f132b542 4849991f "
		"35946729 20a9ce58 821159d0 07d8538b 48f4cde9 b6e7a5d5 47bdca59 e02d2ac4 016df4d1 c0c73858 5a09f133 c7a7e080 56567f8b 1b2f53c2 "
		"3b3e2813 5c0ba11c ac57890b c7e7c641*35 19045051 3048e4c4 59540487 bbd26b4a 3584f691 fb103c46 69b700b2 f543baf1 50b2470d 6c04f4b8 "
		"bb2fd82d ac25f989 c3702bef 10195e72 6f289b0e 0a57dfb5 6f289b0e 10195e72 c3702bef ac25f989 bb2fd82d 6c04f4b8 50b2470d f543baf1 "
		"69b700b2 fb103c46 3584f691 bbd26b4a 898c4459 a5060fcc 792bab79 58643213 b690c75f 21b8f63f be420d47 0434dffb 02e2f8b1 bff58cf2 "
		"f2f761e3 98bfb308 7a752a41 78d08f3b 50eefc1e 890ad4cf e8963864 df352009 436379fc 600bb7ed 436379fc df352009 e8963864 dade3b46 "
		"f90a515c e0781253 8a0e6764 bbfa660c eeb9f8ed 345c549d 8933007a 4e7461e9 d26f6d73 b50cf817 5c7b94a7 b1e9f7d1 811f3ae8 0c38dfc1 "
		"5f387792 18a1dcfc f987643a cbf8a34b d5850d17 a5bfa90d 72eda3c5 e82cbfd6 96361637 93ef4046 90a95ade dc5f235e 6eda1de1 ca1b089f "
		"6eda1de1 dc5f235e 1fa867e5 24e64696 cedbdd72 d740c2a6 4615b802 dea2bd89 f9bc60e3 65ab70e3 60b5c67d 7e27a26d 4486e14a ee480ed0 "
		"8689e647 3da982d6 60f95903 997da887 637315ab 5f6b9f6f 4e8b4d49 c7917646 26844219 9e960fa0 117f3cdc 7b527fa9 acc86928 25040662 "
		"449776b1 e9013dfc 92a064c1 8c7927f4 b9096338 ef6ad7e4 37fb46b4 552ec733 7e5fe3df c154931e cfdbd399 fe050d2d e63375d9 24d3268a "
		"267fd266 f3f63fa5 c36c0c69 c5f21109 fe402fd4 b7ad49a3 2582d118 87f56173 f00d1102 327b6054 908a61b4 57f9d5c9 252bed10 df7bc8f2 "
		"998fc3b3 8e39f823 c4af8296 c7bca4df baab5db0 02fdb859 129c7674 102e5200 c968b8f0 9a66f564 dc350fe5 9e341941 4f3eacaf 87205646 "
		"a877b2e0 e453b770 20397520 96f97a1c 8e41a80c c09d7044 d26a1fb9 2d8b7e02 c0addc4c 036f44fb 07553b40 be943fc3 d368a538 b02f16a2 "
		"5404bdd7 5ca67215 1a6e2a43 5f190ec8 188d0991 454ae8aa ed3f2d0f 6a647db4 6e706740 fe4b8a60 4b38728d cd29ce71 4b38728d 79635a05 "
		"9b3bb7a2 924dc00f 00115027 3258e668 d568caad 67d18db0 6518b334 f1faffa9 fef1aff3 2b9355a8 0f0d4d28 bd502445 0ca182f0 5af13e3c "
		"ce25aec6 c0a5cce1 f2213ef3 c55b0ae4 900b7f4e 9b9b5bf2 200f3d4e 4aa6511f a4423121 1fc87f6b f6199779 6e1f323f c2df63f2 a4febfec "
		"834ddaa8 e9705b2e 834ddaa8 a4febfec c2df63f2 6e1f323f f6199779 1fc87f6b 4cfbd674 48f1036f 564cff33 7f377f5f bfbe878d 2f533add "
		"8ed13bbb b8c90fd2 757e6f72 ee4dafcf 46dd01a5 ba11f4ae 6d4813ef a20b6480 a4eb19f5 f66b0fce 4e7ddc6d 871156e9 9d5be641 3258e668 "
		"00115027 924dc00f 9b3bb7a2 79635a05 4b38728d cd29ce71 4b38728d 79635a05 9b3bb7a2 924dc00f 00115027 3258e668 513c6b92 ecd15826 "
		"c7e7c641*30 e3d44a37 c7e7c641 34430b25*6 728f1fd8 0765577c fdfe3f1d 2e06c6ea 60d1e30d f019863a 5708ded6 b4c87f10 e2d1c053 b30c5d0e "
		"0307fa9b e74cc605 905478a7 6cfc0fe9 87e01cce f738e924 8b18ae63 59058199 8b18ae63 f738e924 87e01cce 6cfc0fe9 905478a7 e74cc605 "
		"0307fa9b b30c5d0e e2d1c053 b4c87f10 5708ded6 f019863a 60d1e30d 2e06c6ea c57585df 1f343114 4b55e18f 0463dd7f deccb801 6de7b6cb "
		"7a6a2836 2a9c2c85 461b702c ac8b2a06 9dac2a82 5dbda566 33608d97 404ca8a0 b90aab68 225a9258 12c40b9f 2d558ba3 0e1e4c8e a752400d "
		"efde5bbd bcad36bb dedb9f0c aa85e4ee 2148a70c 370dd63d 893d6b4f 3fe6fa8a 913a3748 211bb3f7 e9907a43 4ec721ba 4bd14bbb 4368f652 "
		"4e850d73 15da9a10 345d8548 e096df1c 10cdd9b4 bdd25369 7e6bc237 d844620e bdf08ec8 938c18af a7d1a2d7 69fb25a9 0097d322 8967fbf0 "
		"7766a5bb 315b8936 70998cdc f738e924 66e168ef 891a4847 33244c8d 533b41ec 313b4b49 f7443528 a1fbf0db bdfac883 8444a3d7 5aa21722 "
		"3fb15457 ee035002 1bef3673 554cfe4f f0cb2974 91076a1f 89305688 0591a857 31d9d21a beac6fad 33fc7aab 0ff0ac40 8cb8d1fc 25f80334 "
		"53929928 3143c9e4 10ebbdf0 5260823c 5d315054 f1fbb993 5d315054 5260823c 10ebbdf0 3143c9e4 53929928 25f80334 8cb8d1fc 0ff0ac40 "
		"33fc7aab beac6fad 9345cedd 684b1120 aafa145d 34430b25*39 6a314c4a 56d6a188 9ae8eecc*73");
}

TEST_CASE("French translation: The Normal Menu saves over a save in its slot, as its program does", "[Content][Menu]")
{
	// Overwrite previous file? Yes: No.1 written again.
	FMenuScenario Scenario{"file_normal_overwrite", EMenuMode::Normal, 1,
		{{0, 45}, {Cross, 2}, {0, 110}, {Cross, 2}, {0, 40},
			{Cross, 2}, {0, 30}, {Left, 2}, {0, 6}, {Cross, 2}, {0, 40}, {Cross, 2}, {0, 100}},
		480};
	Scenario.PlayTime = 0x13579;
	Scenario.Cards = {{{FirstSave(), ThirdSave()}, {}}};
	CheckFrenchMenuScenario(Scenario, "c53584e2",
		"3f4c6d55 ef6008a9 2f8131f1*2 8da80b4e*2 54563e20*2 58e78bbf*2 9025e274*2 ae8873d3*2 c7bd5065*2 077e39fc 3d6efaa7 daeb231a 10d28ea6 "
		"74e5ef9e 10ed15c4 d1b548d8 d65873be 3dc9787a 03301830 57a99e73 a8f1723f 4856fbae 629c8219 9846603d 8b7a78d6 e076bfa4 168e1db0 "
		"1d76dc8f bfeb454d 6a3a4cb4 e2de9f7c 656a3007 df395fc0*7 3204a383 fa9495fe a4610580 09ab412d ea4ebd7b 1aed471d da337088 b9ead548 "
		"d97abf2e a5104025 70215789 19c7aa84 bd121539 506b5d62 f051c4d3 6766171d c65a8cbd d2500594 c5ae4c39 1200a267 763a602e 2184f80d "
		"e4b5c55e af735cae 9e653b49 f86e17c4 2d3f0656 8d77bc58 1dabcb6c 67be1627 b02e0127 a023b96c 68596dbb ffe9eba8 b68c3add 817fd84e "
		"0dd767a7 164582e0*75 801b7599 c558303c ce592782 c322e216 93078334 8561f61f d6a4eee4 9a7f2dfb e7f94e5d d285c166 b1e83ca8 a64eb0b8 "
		"fb665037 f34dc203 4cc55f87 6d8d8126 5d6271ff 157f0c30 2ca6b43f 5e5f810a b3915002 ec61e900 65b01bbd af8df532 63e33f0d 36db5be0 "
		"2f808f3e c2d86f97 9049b310 74d85bed 5ddaba43 833a79c7 72e0734f e60c46fb 1a6fbd54 e60c46fb 72e0734f 833a79c7 5ddaba43 74d85bed "
		"9049b310 c2d86f97 e2cdba40 512bec4a 31a05a91 39bbad71 39b5c4b2 d44697ea 4be4917c 48476c89 80a8da18 b02528f3 b0005f71 25462419 "
		"89533c37 dd804c5d 3a3ff792 4fed0e30 e55acbb7 41a9b015 c600a179 c4e5eeea 3eec28fc 1daaf5af 9a62bf4a fa035386 e6e71903 fa035386 "
		"9a62bf4a 1daaf5af 3eec28fc c4e5eeea c600a179 41a9b015 9c8583f5 fb5a0924 fa16511c 358facae 9ff92a43 101b7716 1929fa32 f1df3479 "
		"f6790130 5e5f810a 0d0fdd25 5d8c7831 1b542975 f68c23f9 5ef2217a b5896cdb 6aafe797 a4043d1b 4a8374ad b572d333 9d341134 68a33cf0 "
		"b91fd161 2adf54f0 a0893ea3 2adf54f0 b91fd161 68a33cf0 9d341134 b572d333 4a8374ad a4043d1b d1b8463f 5b2c3e01 c806ca41 abc0865b "
		"421d2d94 d35cc958 3d962861 9cb62ebb d4345f47 9cb48a4b 30b33bba 4e0d8642 3ebe55d9 ab6870c9 3c50f82e c2d7f527 65f26d35 35e205aa "
		"40ad0ac5 5e0ed36f 39ded38f b0b9dafb 9966b219 0c011c62 c1fe5bbd 850c79c3 7f382aae b4e57874 1bf16044 ba017768 756d829f f5b6456c "
		"2721bd84 477f26ad ceec59e1 28009afb f38ad52d b8339bb0 11898870 03a4ba4e 23212425 4073145b c05450e0*29 2d986b88*60 d168ecdb*60 "
		"85fc5bb0*25");
}

TEST_CASE("French translation: The Normal Menu deletes a save where saving is not allowed, as its program does", "[Content][Menu]")
{
	// Copy and Delete only; No.3 deleted.
	FMenuScenario Scenario{"file_normal_delete", EMenuMode::Normal, 0,
		{{0, 45}, {Cross, 2}, {0, 110}, {Down, 2}, {0, 6},
			{Cross, 2}, {0, 40}, {Down, 2}, {0, 20}, {Cross, 2}, {0, 30}, {Left, 2}, {0, 6}, {Cross, 2}, {0, 30}, {Left, 2}, {0, 6},
			{Cross, 2}, {0, 40}, {Cross, 2}, {0, 100}},
		520};
	Scenario.LastCommand = 1;
	Scenario.Cards = {{{FirstSave(), ThirdSave()}, {LongSave()}}};
	CheckFrenchMenuScenario(Scenario, "98484db6",
		"3f4c6d55 ef6008a9 2f8131f1*2 8da80b4e*2 54563e20*2 58e78bbf*2 9025e274*2 ae8873d3*2 c7bd5065*2 077e39fc 3d6efaa7 daeb231a 10d28ea6 "
		"74e5ef9e 10ed15c4 d1b548d8 d65873be 3dc9787a 03301830 57a99e73 a8f1723f 4856fbae 629c8219 9846603d 8b7a78d6 e076bfa4 168e1db0 "
		"1d76dc8f bfeb454d 6a3a4cb4 7cb00b62 80320c78 c9b415a7*7 d4b433af 518bcf42 2723a368 1b706678 960f41f5 8a05c399 572714a3 046b75f6 "
		"fc71e145 c4237940 3271a2b6 2fc2d0cb 73655889 5a6a05f4 eb68954a 619a2a60 4846879d 4160373c 463de759 9d6c9fce a0ada7c9 2103bd0c "
		"3acbae26 35d61e04 2d0a04d3 033c8915 982fc48a 27315dd2 6f1ff577 bd37e2eb 94108d39 d9ca5701 5d0751c7 205e0163 88b9a820 3ac3bd3c "
		"2380bd25 7f67a9df*75 d953ee4c 7f67a9df 54077924*6 b7033f7a d196dc2b 8e08c9cf 9971ea78 8499aabf 8a3af09b 9f6bec17 7ab60a6d cbf26b55 "
		"c03e73e3 fbfbc617 370381e3 6cae2464 b2e166a3 f27740ba eb59ba2e 0f1ff712 c08eac7c ec873604 1c0e5bbf dff7eb09 37a4c3ea 709bf4dd "
		"7568bd12 3489fbbd 0558c09c 9e512841 0558c09c 3489fbbd 7568bd12 709bf4dd 37a4c3ea dff7eb09 1c0e5bbf ec873604 c08eac7c 0f1ff712 "
		"eb59ba2e f27740ba b2e166a3 6cae2464 370381e3 67188d23 d5691b33 a6e133c5 37a7d276 ccd72ec8 2a69c07a 62cc9327 e8b392d2 a3fbc3e7 "
		"747576a6 50861fbd c774f152 8b5d6013 7f3f7838 ef116db3 541470dc 0ac6001a 541470dc ef116db3 7f3f7838 8b5d6013 c774f152 602432e7 "
		"6f15bbcf 63868c21 4bee30a4 1bf8a684 beaa4307 1f231f46 a0caa8f3 b0232860 3d52a316 6ba98441 545072d3 2075ae6d ab0279eb cb4760cb "
		"07c6c079 8b5b317b 91067f84 17bc2476 c1cedc4a 09575284 1cdb9f5b 58137286 b2052ce2 01bb5264 63ddc5ca 6e4b0ad1 63ddc5ca 01bb5264 "
		"b2052ce2 58137286 1cdb9f5b eb4a65f5 fc71b777 d377b810 793e5f9d 2a43c398 f44ee67f 223799a2 b61a34d6 d70256aa 799e9735 f237767e "
		"b6ca6105 f895739b abcf8b5a 33a72235 df96fab6 228f4871 341a7cca 3927716d d3ae2e42 986b25af 7f17aafb 1528d248 26c49ae9 d497b934 "
		"5d080725 b5e7b8bf 5d080725 d497b934 26c49ae9 1528d248 7f17aafb 986b25af d3ae2e42 3927716d effdd303 1e832915 531cc5d9 f3177388 "
		"a8aafcaa 188311ee 221d0e47 c21f2f51 67b17927 82ba16bf 017f80aa ba212469 b3690a5d 9cdcd1d8 3cc32349 3bf67043 b774bb10 581aef38 "
		"7fd80a84 ee626c2a 81476b1e 45041963 587d174c 52e0bbcb 587d174c 16274dab cc8f58e3 f505ffe6 4f8b4964 49a8a471 73112639 08464c5c "
		"59820451 96876bed 5d11d3bb 6546d10d 828fd8b5 9d9d78e5 c9f1c078 fb58244f 0f11f75d 74e0cfc3 36dda680 277a326a 5aab776d 450a4328 "
		"e671684f da95c495 d51f1dc6 1922d766 60a8ead9 2beaebdf ebe9eab0 cb0a1054 701ac933 56fc61a2 ec6f99fb 13a0953d 794fa8f6 656bdf94 "
		"157ae6f1 cb86f227 8df350d5 a3753b19 9405c4ea 09de5b4a a41f428d 39d1db0f 5ee3607a 9e4c7ffd 47981e9b 09be0a57 f4e2479a 3950c9d2 "
		"06bad3ae 1c52a2f7 8f04b5b4 3d0fc00a 35a3de7b 80d2e07f*44 d47b00c6*60 93b15d29*40");
}

TEST_CASE("French translation: The disc change saves as disc 2 and stays for another save, as its program does", "[Content][Menu]")
{
	// Save? Yes; a free cell of the first card; the grid read again stays; Cancel goes on to disc 2.
	FMenuScenario Scenario{"file_disc_change_save", EMenuMode::DiscChange, 0,
		{{0, 50}, {Cross, 2}, {0, 90}, {Cross, 2}, {0, 40},
			{Right, 2}, {0, 10}, {Down, 2}, {0, 20}, {Cross, 2}, {0, 30}, {Left, 2}, {0, 6}, {Cross, 2}, {0, 40}, {Cross, 2}, {0, 40},
			{Circle, 2}, {0, 100}},
		520};
	Scenario.Disc = 2;
	Scenario.Cards = {{{FirstSave()}, {LongSave()}}};
	CheckFrenchMenuScenario(Scenario, "48accf8a",
		"478cd0c6 09a4e140*33 a355032a 116d7d6e 2f1dcc45 eaf6f244 3659b4fa f20f6c1d bec88b2b 5c970eff e0ccd49d*9 b1ddb190 09a4e140 740ce7d1 "
		"9cf0e9bf a55ec1c6 dafccbb9 aafeab1f e0634e5a 5f287e59 a6379a86 f52d4db5 93819be0 93337cbf 7bc03e7d 3284f5fa a43823e5 02635afd "
		"32889a11 edd74573 3db37b67 8321295c a4a480da c8bcf035 9ee4bdda 087c1b7c 4650ab2d 39d2123e 2015b614 66c51ed7 99ac7c65 b0e1735e "
		"ff50a7dd dee71cc7 f368e321 78929b09 c8709873 ef323119 08145937 7c3bb348*54 e2a4762e 7f32b21d 8d5fc748 771c22ed a51b08b0 5f1cfedd "
		"ecddaaea 12f043c7 c0d925dd 3967faa9 b004778e cca7f7ec cf52a09e 3e5733ef a227bf05 c8854aa8 fdc95aed c60388d8 fdc95aed c8854aa8 "
		"a227bf05 3e5733ef cf52a09e cca7f7ec b004778e 3967faa9 c0d925dd 12f043c7 ecddaaea 5f1cfedd a51b08b0 771c22ed 2c598494 a31a1431 "
		"d944e86a 47a20c80 5abd1223 5dfeda06 ff14f799 b13433ea 5308f101 80e25223 a0ff169c 70f30aeb f59ca7ec 2fb7837e f5da2b94 c52603b0 "
		"0ac8cd74 2424d84d 0ac8cd74 c52603b0 f5da2b94 2fb7837e 269952a7 faf19127 e6c212c8 29a3eb1d b01af401 199171b4 09c7c071 aa4f66fc "
		"1f52368f fe80e0d8 f40dd4e3 46321c9b 3d555bd8 ab9f2efe e2c77cb2 fab6a373 434b8d3c b5fccfe5 4afff773 d89a8466 ce1a4d94 9cb7c6f1 "
		"789dc07c df552207 14d76011 c8897c83 dfe4d36f d2a139ed d0c85c4e 2458a35b 14206a06 ab036bf3 2c40037d d128cad8 5e3b83ce 71225292 "
		"80b8f111 69d918cf 9a21f05b 3082da81 9274b391 3de896b2 d7cd1da8 8bae978f a8fbaf9b 5f55a831 53c9a8a0 09d43b63 9d72211a 1aa90f8a "
		"d6c17b59 a3a6e41d 81cd4ae9 ae469e81 b71aa89a 2b032272 2f434db9 50f1f501 351cafd0 18175d2c 351cafd0 50f1f501 567d8b5c a05db822 "
		"6161db7f 72df4508 4ab59539 4fd13716 583a16c3 38714582 b77cb9a5 6d7dcf34 00789db7 842ff3bd 8cd6b22b 3d8aeb9b d1fd0e36 e5bb932e "
		"8ecd1178 d3663b00 a31a55f1 f20ae7e1 a56db724 60da0873 1744b1ac e054061d 646d553e 4f72a543 e35f872d dfd68c62 9d62c124 83886340 "
		"9d62c124 dfd68c62 e35f872d 4f72a543 646d553e e054061d 1744b1ac 60da0873 a56db724 f20ae7e1 a31a55f1 d3663b00 8ecd1178 e9a8aacb "
		"1928f73e 76e957a6 7d86c7cf f17bd431 ffd4974c 4a1e5b5c 8a14936a 58372123 a83a6fc0 7dca1607 f12d7739 b9339de2 7754ba2b 07048289 "
		"8f53ef3f f1824ac1 d128a8a3 00067fbd d128a8a3 f1824ac1 8f53ef3f 07048289 7754ba2b b9339de2 f12d7739 7dca1607 a83a6fc0 58372123 "
		"8a14936a 4a1e5b5c ffd4974c f17bd431 7d86c7cf f03630a3 304e4f5c b20cb649 a0a9efd5 ba3b0f14 d76ef590 89e0a6f7 03c90e79 668997e5 "
		"09a4e140*35 24ddaa35*2");
}

TEST_CASE("French translation: The Gear screen passes over members without a gear and refuses Chu-Chu's commands, as its program does", "[Content][Menu]")
{
	// Elly without a gear passed over, Chu-Chu's gear made from her own figures and every command of hers refused,
	// boarding locked, Emeralda's Crescens piloted by her grown self, R1 wrapping past Elly and L1 back.
	FMenuScenario Scenario{"gear_members", EMenuMode::Normal, 0,
		{{0, 45}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Cross, 2}, {0, 80}, {Cross, 2}, {0, 20}, {Up, 2}, {0, 6},
			{Cross, 2}, {0, 20}, {Up, 2}, {0, 6}, {Cross, 2}, {0, 20}, {Up, 2}, {0, 6}, {Cross, 2}, {0, 20}, {R1, 2}, {0, 60}, {Cross, 2}, {0, 20}, {R1, 2},
			{0, 60}, {L1, 2}, {0, 60}, {Circle, 2}, {0, 60}, {Circle, 2}, {0, 40}},
		700};
	Scenario.Pokes = {
		{0x1d34, {0x1, 0x7, 0x6}},
		{0x1d30, {0xc2, 0x0, 0xff, 0xff}},
		{0x3b0, {0xff}},
		{0x22b6, {0x0, 0x90}},
	};
	CheckFrenchMenuScenario(Scenario, "81e98813",
		"8576ccf2 ef6008a9 2f8131f1*2 8da80b4e*2 54563e20*2 58e78bbf*2 9025e274*2 ae8873d3*2 c7bd5065*2 9b09b5f1 27d92857 1ea19b6f 32556404 "
		"4a4fa4f2 523976b2 1542e1ec a1c37bba a5237e06 2e198b9e 78e03974 e8ea7136 c4fe239e db7ddbf2 36c16a34 9ef510e7 e3d6617a 51c00c4d "
		"77b30c34 b5b8333e 1be6ae80 a0ac280a 3f279926 f693a9e9*7 28458a64 e3401c8a*5 ffd46c3e ee17ba55*5 0060a1af 8da7a2aa a325335f*4 "
		"da3b3bb6 76fb545f*5 f01afe78 30b6c626 54254cb0 6382fda2 d13ec25d 78e56051 19e62e26 8859c5ff 2f31c4d1 18daa3a2 051ba9c9 d1452e36 "
		"af72fe9e 8147555f 813e6a1f 51ca13cb 090f38f9 2b363583 5045631f 777aa61c da87a378 2eda73fa 577d245e e016a2d4 f098b8a6 42e3368f "
		"7857a43a 3858ae4e dbf15c74 5263992b c5cec5bd 8488abc8 a6487126 a6b30526 38df9356 154e4724 174ad32a 3d1e667b dfe9c269*2 948f3e39*2 "
		"19bdf674*8 229484b5*32 ee25b2bd f67abe85 514e2edd*2 08b8f7a5 229484b5*17 49c8585c 229484b5 98e4e5bb*4 c8235583*2 ef55b81d 59d00535 "
		"c174155f*2 c36512d3 c8235583*17 4a5bc41e c8235583 0143890f*6 ebd5ab27 13d4457c ceff25e6*2 1a88304f 0143890f*17 25752ce8 0143890f "
		"f09d34ab*4 19656559*2 9b61bee9 8157f083 e24df769*2 51a6dca2 19656559*17 53203236 19656559 77c753bd 42fd5f4b 129723bc 2bec4ead*31 "
		"33cad4f6*26 dff90c3b 44c038b0 288969b9*2 48ff9cd9 33cad4f6*17 7dc7268f 33cad4f6 7bdb8584 62977f75 091be54f f81e52a0*7 0b9337ea*50 "
		"3f598b1b 0b9337ea 7f3da7fc 92da7075 5760e1d6 5e2db589*5 21429050*52 b937808e 6718201b a0068c71 e80244d7 64c184a1 5e836cdd f2783662 "
		"02d54c7c a7d30faa a7bc349f 31cf3f01 60b4ea12 54cd25b4 5c6afb26 7f34a846 39de7540 2ccdc17a 06b7fe77 1f627d74 98d2364b 4ce6e8d3 "
		"e44b3caf 68bc0df9 58b37c21*39 e0ea36b8 4c3733be 3acee443 f9f3bb1a 504f7d7e 2382f16a 5b7fcf7b ebe4dfc4 233529d3 917cf1ff 6cb9efd9 "
		"8f8e283d d04eb36c 5c7fb1c4 c829c3a4 fb768313 acccc554 45394365 03c4ed1c 0e8f3381 9a1d5cea 4d61d30d 8719e155 9a1366ad*2 24ddaa35*3");
}

TEST_CASE("French translation: Billy's gear changes its guns and their ammunition on two pages, as its program does", "[Content][Menu]")
{
	// Renmazuo's guns in the sheet's attack; a gun changed, which turns to the second page with its list open, the
	// ammunition changed there; the last slot's list left with Cancel; the pages by Left and Right.
	FMenuScenario Scenario{"gear_billy", EMenuMode::Normal, 0,
		{{0, 45}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Cross, 2}, {0, 80}, {Cross, 2}, {0, 20}, {Cross, 2},
			{0, 10}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 14}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 14}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 10}, {Circle, 2},
			{0, 10}, {Left, 2}, {0, 8}, {Down, 2}, {0, 6}, {Right, 2}, {0, 8}, {Circle, 2}, {0, 30}, {Circle, 2}, {0, 60}, {Circle, 2}, {0, 40}},
		700};
	Scenario.Pokes = {
		{0x1d34, {0x4, 0x0, 0x2}},
		{0x1d30, {0x15, 0x0, 0xff, 0xff}},
		{0x20bc, {0x1, 0x1, 0x3, 0x2, 0x1, 0x1}},
		{0x2120, {0x20, 0x21, 0x33, 0x36, 0x39, 0x3d}},
	};
	CheckFrenchMenuScenario(Scenario, "79e00dda",
		"e7ef959c ef6008a9 2f8131f1*2 8da80b4e*2 54563e20*2 58e78bbf*2 9025e274*2 ae8873d3*2 c7bd5065*2 d4766bd5 462fe55f 6236dbcd 3151a1c3 "
		"09fb6d96 68addac7 c3bad67b 5cfc2c48 278d5b2e 4e00e04c 32b42727 ce95c112 57c4157e d1e1140d 527fd212 a81ec4fa fe96c9ff 57f0da58 "
		"c7ceb154 1727919f 2ac8cb78 76452f3e eae91955 aade7f34*7 1e2f0322 f56543f8*5 1f51d004 0bc4a950*5 9c40b9c2 c4eaa719 bf300f97*4 "
		"ed74068e 4abd9c72*5 4f7bcc87 ec31f211 f0e97e6f fee75563 f60813de 9510a183 6bbe344b 388f69b8 4e27aad3 5f30d3c1 000e6102 9e428f0d "
		"3cdf2cb2 0489487a fb36e286 4a50a085 0f3d9aa5 ae937f21 ae904d2e db3114fc f6366038 f5f5f9be d91fb79d aaa8c378 fc99ac19 48a650f6 "
		"fa1d491a de861c28 05a49c15 94e81787 c545623a ca48e0c0 55e6a812 3fde66cb ce0f569a 9825ab64 aacfc31f 38fd99fa 76f1aeea*2 4b835e18*2 "
		"cb4d32c5*8 8ae621aa*32 36a1c888 3c970780 c47d9f50 9699588e*19 010f35ca 22634676 4f346e5e 76390e7d*4 3c3da13f*5 423ec365 e5089211 "
		"0d94f8ff d8046aab*4 86f21f79 7635efc6 6ede2475 ac202bb1*2 30d3807f 29f12921*6 5e74b8b4*5 4c4c3fad b8318d10 d848fd99*5 f4b945c8 "
		"fd951450 dadd676e 4ae95d42 dadd676e*13 03c3859f dadd676e 7b954234 8a015112*5 188aa382 47ae3e50 cb8c1556 9f53073e 97214875*6 "
		"fce11a61*2 1f148f95 a587b08f 97c70a5d a587b08f*9 a1db0b98 a587b08f 1697ac2c 5510ad27*7 37db3808 5510ad27 c9391e8a 1c32f982*5 "
		"56c51da6 1c32f982 e5f1ca7a a9f793e9*7 56adf54a d311bdf0 482e5e55 5f4e98c9*2 d70cc6e2 06ba2603*26 4a8b99bf 66dfb82f 25f072c9 "
		"15876745 5f67b129 68a26169 f2ac7ae1 c2841705 3cd75799 aca38776 5cc85788 88cc61df 4f2c6108 d24a5c85 7b62d029 495e3951 f2001562 "
		"d8b882a1 5b7d96c4 a98115e4 dbb45694 4e6b7e18 22119e9e 4fbece1a*11 5fdd7b08*28 30c639d1 93a01f3c 7d0b3e9c f8d6a612 5ff0a67e "
		"3b49317e 1b2cb5c1 73bb5e5d 67ca7d32 ec390303 5d668969 2bac0163 73d20b5a 7afe434d f900d50f a25f1ec8 1adb983a 6d182ee8 029cee83 "
		"5774f0e4 856ae3fa 006a122c e10d795f 9a1366ad*2 24ddaa35*3");
}

TEST_CASE("French translation: The gear shop shows each pilot's gear and turns its arc, as its program does", "[Content][Menu]")
{
	// Shop 9: Weltall shown, the arc moved round, the pilots' gears switched by R1 and L1, out.
	FMenuScenario Scenario{"gear_shop_arc", EMenuMode::GearShop, 9,
		{{0, 40}, {Up, 2}, {0, 4}, {Up, 2}, {0, 4}, {Down, 2}, {0, 6}, {R1, 2}, {0, 40}, {R1, 2}, {0, 40}, {L1, 2}, {0, 40}, {Circle, 2}, {0, 60}},
		300};
	Scenario.Pokes = {
		{0x1d30, {0xff, 0x1, 0xff, 0x7}},
		{0x1924, {0x50, 0xc3, 0x0, 0x0}},
	};
	CheckFrenchMenuScenario(Scenario, "b953f8b3",
		"408a818a 29ddf54d 2008ffc6 ad83c3ee cf82b142 561246db 6dfe1786 23173b9e 46ef7b13 8674b857 ffbb1f39 a65deeca 5b97b6d6 a2ebf2db "
		"2df73fcb dee9f99e ab53f801 dc554fe4 9b64e342 0c805b60 31361ab5 890fc65b 7cdff0c7 b448592b 09ad533c ba6b0450 8605f985 65b6e79d "
		"36201c64 4ce50eb5 10e11d49 59ec6ad7 f47ea913 9d98ffec 7e1e85fb ca55b7f5 ea353de9 3b1e3b9c 7b42df24 93b5cd82 403f44a3 528f1099 "
		"818edd04 3c30a0f4 9621f434 e8251eb6 8df06990 55969115 7a95491d 5e91e9d1 88c2add2 e051d640 37ceb81a b7926e1f e074eef6 63909360 "
		"eddb01ae 650b9b4c fea06c3b 14c4ddba eceacaf9 f3e68422 f78ec726 9820ef3f 90819bf9 6327f920 dd3b1583 9054f649 666855f9 53366bdc "
		"b835a4db 902f0548 a3f88c59 569275be 5ae28d84 32b584f7 c50f8ae3 1daa60c6 fc5c0eae 5cf6f297 0ae38629 83d7646d 1fd6cda5 a7504938 "
		"1cf45209 f2053d5b 1e549465 5361bdd4 dfa38943 480e1493 2faa0b52 5031550a a9ef4d23 c18699c5 ea78c73e cbf9462d 185186ec 384f661b "
		"731980c5 624dd894 de112adc 3b22810f 6ee7a2de 25f7033e 56b231d3 bd74c4a0 05162171 b9139ad3 e06df15d cba475dd 04291b88 83cb4dad "
		"10d2d9c2 bc92c30b cadb4fcc 193a74d3 1afa62bd a93eba90 f09723aa 14395697 7c5459b6 777fb3ec d0cc392c 359e2f32 787a6d9b 7c57206e "
		"4745960d 70f68fb5 f5da2b99 7e9074bf 9f5fd8b4 918f4459 ceffbd03 aa22aedc 2aa8ce7a 4a00c107 4d1cf35d fcdd77b5 a1594f78 d04deaa0 "
		"b9b4cd82 b2bab399 a0c230d6 c55cf8b2 f4209543 c1dba424 463f419e dd1a4424 51968be9 166af5c1 fa2084c2 c1e3ab99 63cab840 373b84ec "
		"6ca45b8d f0693b43 a8c8d914 b484adde d9ef3650 0a8981b1 3b41e3a8 d596c900 10182968 cf8e7a18 9d18eda9 ef95db11 ed527a3a eb9804d8 "
		"8c5471ae ff0c9d66 0bb30d27 769949ba e7f668a4 c9c2cc33 db88d7ac ec0edb7c 21f39a38 e7c8cda3 88bec938 b8023167 9fd665ec ae2c3c80 "
		"110ca358 f5f30285 c88d1701 9dbe8f56 ad2edc32 2f55ca30 1580495e b8e4516e*3 51c24c5d*2");
}

TEST_CASE("French translation: The gear shop opened with no pilot shows Weltall and tunes it up, as its program does", "[Content][Menu]")
{
	// Nortune's parts shop while the gears are away: Fei and Citan joined, neither holding a gear. The first pilot is
	// looked for past the sixteen characters, to a gear byte of the gears' records; R1 switches nothing; a frame fitted
	// to the gear shown, a part bought with no faces to compare, out.
	FMenuScenario Scenario{"gear_shop_no_pilot", EMenuMode::GearShop, 7,
		{{0, 40}, {R1, 2}, {0, 20}, {Up, 2}, {0, 6}, {Cross, 2}, {0, 30}, {Down, 2}, {0, 4}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 70}, {Down, 2}, {0, 4},
			{Cross, 2}, {0, 30}, {Left, 2}, {0, 6}, {Cross, 2}, {0, 80}, {Circle, 2}, {0, 30}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 30}, {Cross, 2}, {0, 70},
			{Right, 2}, {0, 4}, {Cross, 2}, {0, 30}, {Left, 2}, {0, 6}, {Cross, 2}, {0, 80}, {Circle, 2}, {0, 30}, {Circle, 2}, {0, 60}},
		700};
	Scenario.Pokes = {
		{0x1d30, {0x5, 0x0, 0xff, 0x7}},
		{0x1d34, {0x0, 0x2, 0xff}},
		{0x30c, {0xff}},
		{0x454, {0xff}},
		{0x1924, {0x50, 0xc3, 0x0, 0x0}},
	};
	CheckFrenchMenuScenario(Scenario, "0d29f495",
		"fc6c9746 29ddf54d 2008ffc6 ad83c3ee cf82b142 561246db 6dfe1786 23173b9e 46ef7b13 8674b857 ec9f0c78 3db193d1 42a47f33 76e09288 "
		"939e5f50 20e5a99f 11106cea a4e06cd5 90d317ee 60eb0349 9f959d01 267481f8 2e57645c 8be2b289 ba46dae0 fe4f1c8e 0e7ae501 3eee43f1 "
		"86cef74c ffea16b8 53e6d3e8 35dcb36c 073e3590 a26f8a2a ec4d92d7 4dc23767 c3f86427 a16e3c08 ba366ca1 e246f67f 44136093 16a7e95c "
		"235584a2 c048b942 f90bd4c4 2d1821ee a831dc06 a10ae2b5 b9ce1e87 8fee913d c22ce50e 9d7f43f5 218ba1e1 bffcbd30 5055fdf9 86a385fe "
		"07f05668 55962ef7 4c74a76e 7f85c91f 47d47b55 aa566b43 ccb38b8f 48799cf1 2074074f 876c7ecf 60207932 9bc2460f 1c40b043 2afa725c "
		"e97f45f2 76620f74 0935de5a 4c805ced 5f4bc845 5689c8ce 8adf0de0 e823fa95 6f5c0d50 94ee7d13 dbbe289c c4a7b261 6be64c40 0d019c2e "
		"997843ee f479a9af 4bed244b 3773ed58 024a0b46 a975077c 5a585989 e3b35922 cf3af7fe 6f6a118e 064d5900 5c91207f 9c2b6444 296df9a4 "
		"b9256a6e 6189357c f65dac0e e014e9c9 df790c59 1a87225a 9e1869d6 46aa073e 50d7db29 e2ea1967 1a0ae99d 668fda0b 1268fc12 7ebd764e "
		"d00cc5f1 d3035497 f5a5c2a6 494a13df ec96d51b 5cd6f041 58ff4fb6 d6b7eef6 d143461b 666b63f5 ad8971cf 5ca785a2 c934ef7a edab8e97 "
		"31362f7d 80e5e94c cf9f10f9 fa2a0edb fa3c62ab 60b5f6cd ffc78b0a ec5d69b9 678fe156 06f93873 9d1a8996 02d83f5a 26519f35 aa9a2b06 "
		"d101fb6d 0edced97 495f015c bc1ab331 eef2c7aa 2a98d780 1c80a9f9 92b1b01f 944c1825 ed8b3b2f 34601dc2 9980d5fb ee5de681 7b36fcea "
		"5343eaf9 195d2127 22b42ed6 21d5299e 7d2a2750 2e00efdf 3a63dc3a a0bf1453 f3d9508b 5c173277 a0b3ef0a 3da575a1 8be4a22c e0b53ded "
		"476aa349 0d6b3dc7 99a9fd53 81e85a3e 7c0675d1 40be1e78 92d8aad5 9c4a4794 17326a1a c68d3e06 fa212e82 eeac7806 a17c4409 fbf26ffc "
		"c6eb45f1 9d00187f 13ac5c37 eea50d70 aeb20e5e 34e3f75b 02d954d5 90038025 b54c0140 d62a09e4 68caf5e2 02f9f798 9961c613 10d1cf65 "
		"ad10dbb4 f92a56ed aa2e3aa4 19b81347 d3424bf9 b1c2a41d 7a6a1c5b 1932c83f 6d4d9dfd aa42d714 e419aa82 00c8ecf1 9e1a5627 981dc5f9 "
		"544200ba 9d675c73 2d607cb0 fe85daf1 9d58035a 1910264b f0e56a05 badc05fe 841ee988 ab99e957 0e686917 4d91f7a2 84034bfb f2d1483c "
		"6cb4dc74 a974bc84 b8f28bf4 7dd8a69a 1048aa26 5b43c28b 37ffd319 a9027ecd 033b414f 0e2e6fa2 45c45ff9 4b66c988 833e3b12 33fcaf34 "
		"66fc2263 0a2dfdf8 965e8f7e 6619f326 6c656373 791b6fa0 c5054e4e d694198d 8275e63e 07e8c2c8 e7222e06 6c385f99 d7e10549 6a4cb90d "
		"8261a58e 2cee9d82 6f742ef9 b4eba07a d6ccc9d2 37e51c83 8e5a6235 6bef0da4 3f8698da f4daa8b1 581ad32c b36bedf3 3e9691b5 8c2cf7d8 "
		"f47a332f 9ba1efd5 df9f07be 5e8c33de f1f360ac b2875667 0b57a4b8 04d69df2 d511359a d85f6f6a 27c1fabe a54a07db 29d47f8f 7f584ae6 "
		"e2206772 90207c47 c992acea f034a24a f8fe1a96 97e96731 923135b5 193903c4 60a6717b c6f1e91c 975f708b ad7d2b2a da143c25 6adec0b7 "
		"1b9841b3 875b3ef0 abafa4be 06ff9fe3 0966a324 373eba31 91cf840a 1f5c8c00 9f6e5c29 8c585512 ad4d4c6f 5465e8dc c746d92f 5a8cdfa5 "
		"644d996f 27568a26 c7bc2ddd 267340f5 9b462cac 4d120f4c fd3b0bd7 7a5f77fa 91681378 dea943ec 99f1d070 a166796e c653af63 f0390c37 "
		"838efdf9 93b9f1cb ec42624e f2d05d07 ba739326 5561cd2a fe1dcaf0 db1bf458 fdc2e8fe 934b687a 9c8231fc 5b453fc7 6c500271 17270bf8 "
		"9af9146b ff13da5f ed2a5a75 773f0966 e34f6b12 204e7464 9e4b5c14 43b71a26 a7ae49ff f56450f7 0f96cefb 8bded200 97bc7143 4aa65613 "
		"c377c5cd 6edb372a 282f70d5 8860c342 92718ea0 ebcbdb81 d651853d 6ed710bc 532f4cc8 5c71ef0a 0538d6d2 4049edcd f8494268 8e8c037b "
		"c648c5ac 2a8a0d43 2ff6bc54 ca137ee6 df9e6cea 696c4d06 d002f249 ad379f62 ade946e1 8787a87f deb54c3c 9cde286f 44f3c326 38c77206 "
		"2b013e2c 5ed3f7ea 21a2ee62 49c0d4e8 5c1c23f9 8a017508 179d968c 35a92dc4 240f8f3e f64ccd99 a48a6341 e5a25017 df663fe7 8f2101bf "
		"aa4e0099 760e8cfa 87d22f5c 6ab6558d 6de0d55f d3b94390 72fd5610 6fb9f33d b63dbe16 7714cb52 033af6e8 71b20e4f 0c51bb84 567df12d "
		"f8885977 151988c4 1c4d0a02 fbb5ef5b f0d2498a 81bd18f3 4dfcfaa4 3c3689b1 28eb124e a8c16aaf 2ee0450e ca3ea802 30439289 2b7604b3 "
		"b8193143 70f790e8 05b8e2a0 37042cc7 9fcfa774 0db9ee3c e420a9d5 6abb90a6 358ca7d1 8822c1bd 11651625 daaac5f9 cd897146 2372ac56 "
		"584a9ac1 760939fe 45c70890 b38c9640 c14b280a dd191d9a c940f3ba c0694649 826a0e3c 8089ff47 fbb503c1 451e4a47 68a48e18 cfce0164 "
		"ac04cf16 682cd86c 9213685b 1d6671a9 a40be859 af5b82c2 b08246c9 1e242e48 5a9b3ba9 4895966e f686b5a1 6f0f9773 53c6b175 58eccf02 "
		"fa6ad6ad 66633de4 dddc63ee 1d5c5544 8128d181 fe8e9c04 9881bd23 152666a9 0e2b2c35 c00bbaf5 abf6b3b1 b0d458ba 19db5340 1a743dea "
		"c8622eca d384d48c ddb9f69a 84ec5b61 cb6a1323 05c9817f c285618e 58f9912e dddee05f c8779fea 40d7e3ad c7c3ea0e 1f9ebce8 1d8f0861 "
		"0ba68ee6 addddada 81562494 e2f7cf30 760611ff 60533b89 eba508bc fe1fe114 61f5c43e e4162dbd 64eda171 487b3535 5ba3d55b 65a62cb7 "
		"4fcec4cf fafcc2dc b6afff64 bf6b6365 66e2b3e1 85527e0f 5a7e6178 103f16c9 85878995 a4b591b3 5740a1d4 4cf32117 3a297465 d8840ed2 "
		"814a9e06 4ebecdf2 2b47e614 f527fcb6 90787b1c 748fac98 2066379f 09599509 0249ffc8 f99bb59f 1b93c3a2 e980b6eb e486beba 47a3b9d1 "
		"b2ac8bfc 37f92333 1b6bc7d0 f48cfaa1 55b50134 1ddb67ca 5231cc88 fe140b73 0b7c1703 475bd6f4 bdd765b4 d4199505 0e123d30 dcdfe46d "
		"f6e80ce2 e303498d de94e3fc 92b7d9a7 c8892be9 20f6c9b2 bf4435da 7de7f7df d7a7c817 5a6855f7 65a7be07 e79fc15b 06e58651 4f7cbbc1 "
		"c95e9acd 309c3200 d6550099 1fb3b915 580034c6 a1bf101f 6485b764 9d053436 57007208 c6af8334 8639f39e f57acb2e 3f1fcad2 e11899c4 "
		"176689e3 7404992f 29888138 59c5e472 d8eb90be f1b331be 344a0dcb 6d0f505f 35578089 9e4bceaf 84a1b91f 2777aae7 b7b33440 afee5934 "
		"9e7851f9 388f4097 f26d6ce9 1759b548 5c8292ec c7926793 c91a74e5 436a3968 50ecd076 a33d4fc9 d9455a6c b04981a6 80c3a8c5 09c3a84b "
		"f4970c6f 147f4c68 2a74e2fb 554e60e5 bc7d7eb3 b34d8744 849715c3 1957e07c 07f2fde1 708023ae 9b9fea37 04e30f5f f98b15e8 dafb2933 "
		"ca386170 fe5fd131 f300a785 cfed0ef2 30cbc461 2703ace8 942a43d4*4 51c24c5d*2");
}

TEST_CASE("French translation: The Items screen applies every kind of item effect, as its program does", "[Content][Menu]")
{
	// EP (on a member already full: the buzzer), a stat, max HP, weight gained and lost until the slot empties, HP and EP
	// together, and the data's two debugging items.
	FMenuScenario Scenario{"items_effects", EMenuMode::Normal, 0,
		{{0, 45}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Cross, 2}, {0, 60}, {Cross, 2}, {0, 8}, {Cross, 2}, {0, 20}, {Cross, 2}, {0, 10}, {Down, 2},
			{0, 6}, {Cross, 2}, {0, 10}, {Circle, 2}, {0, 20}, {Right, 2}, {0, 6}, {Cross, 2}, {0, 8}, {Cross, 2}, {0, 20}, {Cross, 2}, {0, 20}, {Left, 2},
			{0, 6}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 8}, {Cross, 2}, {0, 20}, {Cross, 2}, {0, 20}, {Right, 2}, {0, 6}, {Cross, 2}, {0, 8}, {Cross, 2},
			{0, 20}, {Cross, 2}, {0, 20}, {Left, 2}, {0, 6}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 8}, {Cross, 2}, {0, 20}, {Cross, 2}, {0, 20}, {Right, 2},
			{0, 6}, {Cross, 2}, {0, 8}, {Cross, 2}, {0, 20}, {Cross, 2}, {0, 10}, {Cross, 2}, {0, 20}, {Left, 2}, {0, 6}, {Down, 2}, {0, 6}, {Cross, 2},
			{0, 8}, {Cross, 2}, {0, 20}, {Cross, 2}, {0, 20}, {Right, 2}, {0, 6}, {Cross, 2}, {0, 8}, {Cross, 2}, {0, 20}, {Cross, 2}, {0, 30}, {R1, 2},
			{0, 10}, {L1, 2}, {0, 10}, {Circle, 2}, {0, 60}, {Circle, 2}, {0, 40}},
		900};
	Scenario.Pokes = {
		{0x1f90, {0x2, 0x1, 0x1, 0x1, 0x1, 0x2, 0x1, 0x1}},
		{0x2026, {0x6, 0x33, 0x22, 0x37, 0x4a, 0x48, 0x31, 0x21}},
		{0x404, {0x5, 0x0}},
	};
	CheckFrenchMenuScenario(Scenario, "093c6b5f",
		"3f4c6d55 ef6008a9 2f8131f1*2 8da80b4e*2 54563e20*2 58e78bbf*2 9025e274*2 ae8873d3*2 c7bd5065*2 fbca6d43 19414b8f 6d12ac05 0652f856 "
		"f3d88d87 29dfb538 a6829478 e21e368b 560edc7b 43fca00b edba4c2f ede178bb 352987f4 616413a3 338c468b 0b567317 378a7e26 d43a490f "
		"36203bf6 05d2758c 3aacd550 a85a70ac ebff9f71 9a0b4b77*7 b4a959a9 68f73605*5 ea89e104 49b80875*5 62f34885 88841683 433658e7 "
		"34c4f9fb 8fc42a01 2c873cc0 a10547f4 335290ff affeea00 97430f70 1647f29d 8809964c 1290ed33 f83cae5d 98a46f02 d3962fe7 3cff460a "
		"5ab172fb 1afbeb15 e5ed0f8b 96d9577d cc27ad6b 5737637e 80adc7c2 a4e32d44 f7e552e2 163941d5 1992a6bc 1f3a8b61 c661357c 210840ec "
		"93cc470b fedc1227 abb3320f dbe370fa c8b6d931*6 3c5717f7*6 245edf35*6 48366f55*6 cb4e50d9*3 6a198f37 92eb8c74 32029ca7 9e9478c8*6 "
		"05075dae 4a41c825 4b87d094 e169af20 2e74ad11*19 e40c49d0 acfd8bca e169af20 2e74ad11*9 b8a44ae5 2e74ad11 d72759e4*6 4706167f "
		"62978dcc 02775a60 0c293ea0*5 1a1cc831*4 cf2f1cdc 1a62dbf9 4023a457 baa4354f*3 42278423*6 2193e80c*6 d4d0e1eb*4 8e8fafa0 d4d0e1eb "
		"e50b7426 110df6b4*5 17d73cfa 5fd65599 bdca823e*4 73a9ce21*4 3c748d09 93a4e5fc 152054c8 865262b0*13 5515c804*6 83dcd24a e7ddd74c "
		"4a1bf5ff 3a0ad0a9 b124bb5c c50fe446*5 98c82754*6 a3699246*6 359c2b27 c44a1068 9977ff28 e621666b*3 cb0767de*2 db803f57 cb0767de "
		"6740f7d7 9ec28c37 4b164675*4 4da3a3c0 4b164675 92971ecd*6 ac98c70c*2 259be3db 89d2dc5d 02660e81 bbeaaf0b*3 ac90e72a*16 5144f4b8 "
		"8e05ccaf 093d351f b64e10a0 91863c5c f9b30d19 3a9d92a4*6 83c3c8f3*6 6de14fd2*4 ca7cb408 6de14fd2 5e4ac790 282dad29*5 1399a965 "
		"2e259b41 a61775ed*4 984e96b2*4 1e534b4d 0dd2f55e b58b6454 1f5464e6 ad87451f*18 bf1f087f b0ccce5c 9c82da87 d965f071 46e7d87a "
		"3ac0fc1d*5 316df16b*6 282d5893*6 9fcae970 0f805720 fafbf93d*4 90b4f88b*2 2d6c7261 90b4f88b 51a998d8 36dd8e15 13f8cb11*4 d8b49dfa "
		"13f8cb11 9a1ec593*2 92582e35*4 cabebef9*2 6c3dcba8 d2408677 22ef9d73 3177ff43*19 51a9f479 4ff819af f4d4d540 7fd23493 e3950c91 "
		"dc8e6bc7 e0e4607e*6 e4034fc4*6 8a808721*4 0b3fa15b 8a808721 64c08a8d cbd34b32*5 01e0bc82 e7544909 9711226c*4 9caad2c2*4 32cbb995 "
		"812ff5d6 202f96c9 12a4a403*19 a7598edc 3be59eeb d3fee7fe cb1d0f1e*9 2c60210c 0b2046c0 b99e1f02 ac36be9f 1892ee3f 502047b2*5 "
		"468d1828*6 cf480b48*2 94c719bf*4 b1abcc1d 8b14c255 d15f38b4*4 da2de6ea*2 029baf76 da2de6ea bab8f927 a8de098b 537a283c*4 9fcb77a8 "
		"537a283c 0d9badf7*6 bfb46fbb*2 d4577e52 8e6e1a11 b910dae2 5edae763*19 e70d326f 20765a40 b8c12b18 6064e9ca 53ee09ec cb92435e "
		"b1887e0e*2 abc629f4*4 e118c20e*6 534d4b30*4 8ed0b991 534d4b30 6e0a98f0 2954c30e*5 9a55fd8d 7b9404f8 ffeb8c2e*4 3b1f507b*4 b2fa5105 "
		"3693cae6 8a462a3a 5b072622*19 7270149c 21effbb5 d7737093 dfa0ba3f*3 efbdafe8*26 98a056db efbdafe8*11 98a056db efbdafe8*11 439dc518 "
		"c653c61b b891e3f3 8619872f*6 373d0aca 2712fd0d*5 fb0b51d3*6 f641e383*6 db6ff492*6 0a6077c3*6 2712fd0d*6 fb0b51d3*6 f641e383*6 "
		"db6ff492*5 494f19c8 c4ceb146 54f9f60a 0c3e4b16 18afde70 783c9a31 a2993d64 0d005929 876f0aa5 c00b4adc e17c1d85 62d94861 2d12fb46 "
		"b19c9e85 f9aa7543 e7320547 85ec84db f6713dd3 ee8615fe fa443d79 ebb68602 e7e61734 2745b4fb 487da4b5*45 ace12171*60");
}

TEST_CASE("French translation: The title screen reveals its commands and waits, as its program does", "[Content][Menu]")
{
	CheckFrenchMenuScenario({"title_idle", EMenuMode::LoadGame, 0, {}, 120}, "a851da6a",
		"b4ead516 ef6008a9 503a6645*2 4b77ef5f*2 db3467bc*2 261cb849 3ef16831*111");
}

TEST_CASE("French translation: The title's Sound option opens on the output mode and moves, as its program does", "[Content][Menu]")
{
	CheckFrenchMenuScenario({"title_sound", EMenuMode::LoadGame, 0, {{0, 20}, {Down, 2}, {0, 4}, {Cross, 2}, {0, 40}, {Up, 2}, {0, 30}}, 160}, "6b823e1e",
		"b4ead516 ef6008a9 503a6645*2 4b77ef5f*2 db3467bc*2 261cb849 3ef16831*12 e74acfe3 f2ba59f1*5 6ac04b34 988bf767 3beb5f22 6a04e735 "
		"0cc82df8*2 4cff2972*2 ea6f1899*2 c3329107*2 0f258d16*2 75d7ea4e*28 e84c33aa 75d7ea4e 0ec3c4f5*89");
}

TEST_CASE("French translation: Cancel closes the Normal Menu, its panels sliding out, as its program does", "[Content][Menu]")
{
	CheckFrenchMenuScenario({"normal_exit", EMenuMode::Normal, 0, {{0, 60}, {Circle, 2}, {0, 10}}, 200}, "bbb57bc6",
		"3f4c6d55 ef6008a9 2f8131f1*2 8da80b4e*2 54563e20*2 58e78bbf*2 9025e274*2 ae8873d3*2 c7bd5065*2 077e39fc 3d6efaa7 daeb231a 10d28ea6 "
		"74e5ef9e 10ed15c4 d1b548d8 d65873be 3dc9787a 03301830 57a99e73 a8f1723f 4856fbae 629c8219 9846603d 8b7a78d6 e076bfa4 168e1db0 "
		"1d76dc8f bfeb454d 6a3a4cb4 7cb00b62 80320c78 e307975f*21 5f1ea30b 8c396597 019bcd08 1c0af9e2 fb1fed50 f95f0838 c30ef074 a3e6e792 "
		"c3d07bfc 42a1a35a b256f78b 85402b57 fc8b4612 50ad6ced d3510b5d a715fc3a 347ef229 403fc95f cb89f982 3c6d24a4 5b28002e 844bf327 "
		"6b76f550 5e1f3998 b701ee60*2 24ddaa35*2");
}

TEST_CASE("French translation: The Normal Menu's arc moves, repeats and wraps, and remembers its command, as its program does", "[Content][Menu]")
{
	FMenuScenario Scenario{"normal_moves", EMenuMode::Normal, 0,
		{{0, 50}, {Up, 3}, {0, 3}, {Up, 40}, {0, 5}, {Down, 2}, {0, 2}, {Down, 2}, {0, 30}, {Circle, 2}, {0, 10}}, 240};
	Scenario.PlayTime = 0x123456;
	Scenario.Seed = 0x2468ACE;
	Scenario.LastCommand = 2;
	CheckFrenchMenuScenario(Scenario, "0af3e34f",
		"3f4c6d55 ef6008a9 2f8131f1*2 8da80b4e*2 54563e20*2 58e78bbf*2 9025e274*2 ae8873d3*2 c7bd5065*2 077e39fc 3d6efaa7 daeb231a 10d28ea6 "
		"74e5ef9e 10ed15c4 d1b548d8 d65873be 3dc9787a 03301830 57a99e73 a8f1723f 4856fbae 629c8219 9846603d 8b7a78d6 e076bfa4 168e1db0 "
		"1d76dc8f bfeb454d 6a3a4cb4 ac68cf98 69964055 1149c7bf*12 e9a08e5d 23b76462*2 5edff11d*3 0ae17d52 1e6f0b50*32 fce3de84 c129caf7*3 "
		"384505f3 7ba3a3ce*7 b8fe74a8 c129caf7*3 384505f3 1e6f0b50*7 5fe8a63d*24 017eeb32 8bdf6a3d cf6e75fd a646a324 fe56abb3 85befeef "
		"e93dddfe 9c531f52 dbb87791 0f8d468a 130ad1bf 082c57c0 6f7d47bc 4ff46d5c 7b053aa8 c938189c a9b434fc a0517049 b674193d f7c869c7 "
		"0a6e49a5 966f2797 97ca02a2 ac6e6dd0*2 24ddaa35*3");
}

TEST_CASE("French translation: Where saving is allowed the Normal Menu opens on File and keeps no command, as its program does", "[Content][Menu]")
{
	FMenuScenario Scenario{"normal_saving", EMenuMode::Normal, 1, {{0, 45}, {Down, 2}, {0, 5}, {Circle, 2}, {0, 10}}, 160};
	Scenario.LastCommand = 4;
	CheckFrenchMenuScenario(Scenario, "088de778",
		"3f4c6d55 ef6008a9 2f8131f1*2 8da80b4e*2 54563e20*2 58e78bbf*2 9025e274*2 ae8873d3*2 c7bd5065*2 077e39fc 3d6efaa7 daeb231a 10d28ea6 "
		"74e5ef9e 10ed15c4 d1b548d8 d65873be 3dc9787a 03301830 57a99e73 a8f1723f 4856fbae 629c8219 9846603d 8b7a78d6 e076bfa4 168e1db0 "
		"1d76dc8f bfeb454d 6a3a4cb4 7cb00b62 80320c78 c9b415a7*7 a0587296 aa3e7b85*6 3cdbc132 2e3798c0 d838b4ad 37e72dab 6b3d6a43 673e57ce "
		"bbc49360 8159b095 d494082a 0f1b7c8f 8b456dbf 895f9ca8 e3be7db7 8b7f0c37 3a5b7eb4 1a5c84a0 d110b380 f03c3282 b159ca01 7099cb6c "
		"32b9861e e3467c52 6ad4dd5d c1b56f53*2 24ddaa35*2");
}

TEST_CASE("French translation: A shop's items past the gold are refused and a cart can be given up, as its program does", "[Content][Menu]")
{
	// Shop 8 with little gold: rows dimmed past it, the cart refused there, the purchase and the cart's cancelling
	// answered No, then the cart given up with Yes.
	FMenuScenario Scenario{"shop_items", EMenuMode::Shop, 8,
		{{0, 20}, {Cross, 2}, {0, 70}, {Right, 2}, {0, 4}, {Right, 2}, {0, 4}, {Right, 2}, {0, 4}, {Right, 2}, {0, 4}, {Right, 2}, {0, 4}, {Down, 2},
			{0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4}, {Down, 2}, {0, 4},
			{Down, 2}, {0, 4}, {Right, 2}, {0, 4}, {Right, 2}, {0, 4}, {Right, 2}, {0, 4}, {Circle, 2}, {0, 30}, {Right, 2}, {0, 6}, {Cross, 2}, {0, 30},
			{Cross, 2}, {0, 30}, {Circle, 2}, {0, 30}, {Circle, 2}, {0, 30}, {Left, 2}, {0, 6}, {Cross, 2}, {0, 40}, {Down, 2}, {0, 6}, {Circle, 2}, {0, 60}},
		800};
	Scenario.Pokes = {
		{0x1924, {0x96, 0x0, 0x0, 0x0}},
	};
	CheckFrenchMenuScenario(Scenario, "8f0ebe39",
		"1b6d91ec ef6008a9 14c42477*2 ff9aeb5c*2 f42b8755*2 4c878366 8a1d0b75*13 59e34ee9 3c49b59e 9f6ccacc 455cc443 02ec76e0 69241447 "
		"297a1c6f 2538df61 e99df893 ecf1b014 06bef78d 35497b68 78671ac1 136785f8 c673a4d7 ae312bda b2498f90 f9f0bae9 8cbf0ee6 baa6aa8f "
		"8713ae46 acf26437 ff10221c ac486034 1d7608e2 0d4718b0 f6d38097 ebe5f530 3c18ee62 b027fe32 cbcd49cb f6caa44a 8fb78941 267ddf41 "
		"160f6f2e 47e72ab4*5 06f97213*6 e4c74afc*6 ad3c6614*6 090d6591*6 47e72ab4*6 06f97213 3df57839 06f97213 e80ca206 d3d82b54*2 37f64d81 "
		"e6f21189 37f64d81 473dfeae 37682b26*2 de0f9aa1 43b62ba1 de0f9aa1 b94967eb e2680188*2 658a3148 aec47839 658a3148 ff90729d "
		"803d1f85*2 4cd1d3ab 6b1a45ee 4cd1d3ab 31d25447 16bed661*2 d72487d7 89a89343 d72487d7 2d1e9c66 db218f08*2 70e8a38b e300d749 "
		"70e8a38b 1bf29e92 97691182*2 1a3607da 9012a35f 1a3607da bf17073c 78c516ef*2 cde6b3d1 16dbd70a cde6b3d1 62279d42 a055bed1*2 "
		"cd76edd6 f43505f4 cd76edd6 9b8f202b f4482773*2 78518f9d 9689f1b7 78518f9d 67822776 d89ce8a5*2 83ecf345 59f2fbad 83ecf345 7054ba97 "
		"54db6ecc*2 c8ac2e6e fe0ecedb c8ac2e6e dded9fd4 c6d0a6f2*2 d955057e 07cf6790 d955057e 8de8e470 c77cfc56*2 b815482e 1eaf3fcf "
		"b815482e*4 d356d3b0 fe661f7b d356d3b0*4 1e11cec4 af7b3b8c 1e11cec4*4 8feababd 8cc67641 053d7279 eb389c92 2968e306 62e514d3 "
		"4ca8fb7b c7d4adb1 1fb8ab14 4170e3ed dc6d4a80*23 1391b0bb dc6d4a80*8 59e34ee9 4f6bb147*3 df26b0ef 8feababd*2 c77cfc56*6 b815482e*6 "
		"d356d3b0*6 1e11cec4*6 8feababd fd0536bd eb389c92 2968e306 62e514d3 4ca8fb7b c7d4adb1 dd4af766 722bb138 fc1b9a27*23 b58370f4 "
		"acc6f687 4f6bb147*2 df26b0ef 8feababd*3 c77cfc56*6 b815482e*6 d356d3b0*6 1e11cec4*6 8cc67641 053d7279 eb389c92 2968e306 62e514d3 "
		"4ca8fb7b c7d4adb1 1fb8ab14 4170e3ed dc6d4a80*23 1391b0bb b410514c*8 59e34ee9 acc6f687*2 aa0efb55 a0cbb71c*37 9ac33fd8 05e8b156*7 "
		"a34f2854 05e8b156*2 24ddaa35*2");
}

TEST_CASE("French translation: A shop buys the party's items and a member's accessory, as its program does", "[Content][Menu]")
{
	// Shop 3: items (a key item left out) marked, unmarked and sold with Yes; Equipment: Citan chosen by the faces and
	// one of his accessories sold.
	FMenuScenario Scenario{"shop_sell", EMenuMode::Shop, 3,
		{{0, 20}, {Down, 2}, {0, 4}, {Cross, 2}, {0, 60}, {Cross, 2}, {0, 60}, {Right, 2}, {0, 4}, {Right, 2}, {0, 4}, {Down, 2}, {0, 4}, {Right, 2},
			{0, 4}, {Left, 2}, {0, 4}, {Cross, 2}, {0, 30}, {Left, 2}, {0, 6}, {Cross, 2}, {0, 40}, {Up, 2}, {0, 6}, {Cross, 2}, {0, 40}, {Right, 2},
			{0, 6}, {Right, 2}, {0, 6}, {Cross, 2}, {0, 60}, {Right, 2}, {0, 4}, {Cross, 2}, {0, 30}, {Left, 2}, {0, 6}, {Cross, 2}, {0, 40}, {Circle, 2},
			{0, 30}, {Circle, 2}, {0, 60}},
		900};
	Scenario.Pokes = {
		{0x1d30, {0x1f, 0x0, 0xff, 0x7}},
		{0x1924, {0x64, 0x0, 0x0, 0x0}},
		{0x1f90, {0x5, 0x2, 0x1, 0x1}},
		{0x2026, {0x1, 0x6, 0x4c, 0xa}},
		{0x428, {0x10, 0x0, 0x5a}},
	};
	CheckFrenchMenuScenario(Scenario, "04adbd47",
		"1b6d91ec ef6008a9 14c42477*2 ff9aeb5c*2 f42b8755*2 4c878366 8a1d0b75*12 80020700 05e8b156*6 131a087b ab3a59d5 e871c65c 286c172d*2 "
		"645f6f79*2 ec02e2cf*2 bae6b223*2 bc6f270c*2 6ff5510d*2 0d97fe39*2 04f5a946*45 131a087b 3fce426a 25e1312e 1cd161df e5c57553 "
		"76251407 8d021bbb 13da0483 d0945344 a69d027e 2ed30530 320d0a8c 95962f72 2fdb5cb8 b1330bd3 758db5f1 12518585 c1add6a1 42715583 "
		"1a2037c6 1b08ce60 dd49c572 1c824058 2567d3c8 2ff2aca6 77c3f2cf 51d11a5e 791353af 49eab7db 47ab1dbd 362dacef 05c39e8f c929e450 "
		"ff19ef0d 395829ed de37b379*5 34c563ba*6 be3461ff*6 36788dc3*6 94ef3c2d*3 7cdd68b6 94ef3c2d 84b2e52e 0a34569a*3 5c184989 0a34569a "
		"b60dec7f 8e6466ff*3 48140be6 8e6466ff 0e0724d0 e7bf7bb0*3 8eccc6ce e7bf7bb0 6bdc97ac cbf1d52e*3 bca9438c cbf1d52e bedafc3c "
		"d26f0554*4 897a11c3 8b865bc7 e33f4eb4 56d7d03c 6e7cf5c4 becd20df cb0f15e9 f5834b19 2ace21c3*23 67c38af8 803b03b7*8 131a087b "
		"e760d1e3 47c90484 04f5a946*38 49849c97 04f5a946 f3780456*7 a8a3bb07 56f1f913 0c950077 5271f1fb 48272c96*7 e8587dec 9d258d98 "
		"abdebdfd dac7a6ee 3a6adb93 1ee19ce3 cb0833ff 05a7eaf5*2 96eb8080*21 1497f7a8 33a2bd8e*7 4e67fef3 d880f5bd*8 970e113f a6b4670a "
		"c82edd69 2de304e0 04a37cc0 a5f4f57f 72a181ae f8009446 10155e5f 32109f7c b5174600 a3797b78 c01a0111 dae6fdf1 f9f5d2b5 97ac5e8b "
		"15c309f4 99e907aa 7647e906 b4358544 85ffb0e7 4bbbd04b 50d5a7cc 2e04675a 433b2b2e 8fb271d8 59af65fd 76620726 1525c53c 4e8e2110 "
		"c6524276 7ba218a6 38b71622 988f92c2 f71646e2 a4183dbb 2a840920*5 9f3e612f*6 113a8e3f*6 d6b8faf8*6 ac5818ec*2 d8fbbc8c ac5818ec "
		"13338419 95883586 543f93ff*3 897a11c3 8b865bc7 e33f4eb4 56d7d03c 6e7cf5c4 becd20df 4de3776c f58dcafd 95487e36*23 2d2b2f28 "
		"c1c81522*8 131a087b e760d1e3 5177c0c1 f3780456*38 338dfaa3 27d42756 04a32b77*30 3b6ca438 04a32b77*2 24ddaa35*2");
}
