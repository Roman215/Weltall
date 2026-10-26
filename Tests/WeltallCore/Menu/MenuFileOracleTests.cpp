#include "Support/MenuOracleRun.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;
using namespace Weltall::Testing;
using namespace Weltall::Testing::MenuPad;
using namespace Weltall::Testing::MenuCardSaves;

// The File screen against the original module run whole under the oracle harness,
// frame by frame, on two cards the oracle models and the port's first two stores hold: the
// same saves, made by each side's own save code. The oracle runs the original without its memory card's waits,
// as the port has none; a save's place name is drawn from the BIOS's font on a
// console, so its image is left out on both sides.

TEST_CASE("The title's Continue moves over both cards and loads a save, as the original does", "[Content][Menu]")
{
	// Right to the second card's save, Left back, Down and Up between the first card's; Load answered No, then Yes.
	FMenuScenario Scenario{"file_title_load", EMenuMode::LoadGame, 0,
		{{0, 40}, {Cross, 2}, {0, 70}, {Cross, 2}, {0, 40},
			{Right, 2}, {0, 24}, {Left, 2}, {0, 24}, {Down, 2}, {0, 24}, {Up, 2}, {0, 24}, {Down, 2}, {0, 20}, {Cross, 2}, {0, 30},
			{Cross, 2}, {0, 30}, {Cross, 2}, {0, 30}, {Left, 2}, {0, 6}, {Cross, 2}, {0, 60}, {Cross, 2}, {0, 60}},
		520};
	Scenario.Cards = {{{FirstSave(), ThirdSave()}, {LongSave()}}};
	CheckMenuScenario(Scenario, "90b48f38",
		"44223061 ef6008a9 854c0222*2 7c54d75c*2 8172c16c*2 538d6d19 e8e08344*32 236ffcab d1d0ecdc a2c3264b 69093596 be88e75a "
		"759d3cae c7cf8ab8 a2973eda 3dff566e 082cc4cc 7f81bc57 797ab439 1775df06 05042b96 33cb7284 1761dc16 6c2c9bd7 080db7ea "
		"2d7f21db d0d6bf53 4587172f 7a848692 68d67fb6 18032b4a bd4f7ada db9b141f d236e630 eb27b018 437ae240 6a8df147 d27d0ef1 "
		"6e736745 45e7be6e b50101ee 5c8b5c4e 62a9ba62 f655560f 1e197c3c*35 30e7a410 f6fd2882 5d09f2c3 02aa7f05 23eade51 934963a0 "
		"f6b3fb5c fff9d08b 6d1a8175 25396066 afce0ef0 fed9b083 884018d6 b5fd4cfc 7f4db6d8 81b91b33 7f4db6d8 b5fd4cfc 884018d6 "
		"fed9b083 afce0ef0 25396066 6d1a8175 fff9d08b f6b3fb5c 934963a0 23eade51 02aa7f05 05bb30b6 1b22bc02 2a97bcdf 8c089d55 "
		"7b1bcecc 5f588b8e 00148dd7 d9861618 a004c92f ad5bfc2c 3188c3ca 67e856d6 2a329c60 4973ff80 fd1a6062 33f63e0c 38f40533 "
		"f5997c72 7a913afe 5d36d914 7a913afe f5997c72 38f40533 17c5c844 fa16f721 6adfc44c a6b797a6 428359b9 51fab58a f8a524d2 "
		"240ae8d1 c121fd7b 8bc8c4e6 5c30bca0 c94c3005 04b5b57e 214cbad6 a01fce7e f4929df8 bad6e3da 7435c046 fe40972c f6b3fb5c "
		"fff9d08b 6d1a8175 25396066 afce0ef0 fed9b083 884018d6 b5fd4cfc 7f4db6d8 81b91b33 7f4db6d8 b5fd4cfc 884018d6 fed9b083 "
		"afce0ef0 25396066 6d1a8175 fff9d08b f6b3fb5c 934963a0 23eade51 02aa7f05 05bb30b6 1b22bc02 98417730 c50f3d45 af62dcae "
		"e67058cd 797f583c ed87113b 3b58e95f 8036c82b e9b7daa7 4a018ebf b5d5a44b 698a78d6 ef96a09e 3f98bf09 01855943 09a2f624 "
		"5c3f2542 8afe3d03 5c3f2542 09a2f624 01855943 3f98bf09 ef96a09e 698a78d6 b5d5a44b 4a018ebf 84884bc6 c5ec57d7 a004c92f "
		"d9861618 00148dd7 5f588b8e 7b1bcecc 8c089d55 2a97bcdf 1b22bc02 05bb30b6 02aa7f05 23eade51 934963a0 f6b3fb5c fff9d08b "
		"6d1a8175 25396066 afce0ef0 fed9b083 884018d6 b5fd4cfc 7f4db6d8 81b91b33 7f4db6d8 b5fd4cfc b644ae7e 044ce772 f178b417 "
		"2b995d82 313ff466 53652613 0cbe4909 37d495c5 c8abd487 7be6673b e0895c23 ab2aab5a b158c5a8 b89b031a af62dcae e67058cd "
		"797f583c ed87113b 3b58e95f 8036c82b e9b7daa7 4a018ebf d86e99c8 3276c5bd 3d5b3dbe 36179a95 dfe6d3b4 6f7ebfa1 22c59031 "
		"4e195a88 7a583e99 6467da54 c7432171 937b993f e9389007 23100c90 5c7291d2 92d544bc fef3fa79 527a6871 ccc34791 395a0922 "
		"cbc798a9 779a926d 0602822c abdd2954 2e5bf1f3 a85694a4 4d3a659b ae8ad1f3 aabdbf86 4c7d54bd c35b0024 f49db7c0 1a301728 "
		"2b995d82 f178b417 55ebc911 7bac31d9 bfc2afb6 8ecd2555 d452b0b6 8ecd2555 bfc2afb6 7bac31d9 0e9e8588 f178b417 2b995d82 "
		"313ff466 53652613 0cbe4909 37d495c5 c8abd487 7be6673b e0895c23 ab2aab5a b158c5a8 b89b031a af62dcae e67058cd 797f583c "
		"ed87113b 3b58e95f 8036c82b e9b7daa7 4a018ebf d86e99c8 3276c5bd 3d5b3dbe 36179a95 dfe6d3b4 6f7ebfa1 22c59031 4e195a88 "
		"7a583e99 6467da54 c7432171 937b993f e9389007 23100c90 5c7291d2 92d544bc fef3fa79 527a6871 ccc34791 395a0922 cbc798a9 "
		"779a926d 0602822c abdd2954 2e5bf1f3 a85694a4 4d3a659b ae8ad1f3 aabdbf86 4c7d54bd c35b0024 f49db7c0 b5bde533 edeaef0e "
		"093f8afe 7b448f43 06a0cb7a c60fa3eb abbe0b22 fcab3674 efa03095 bfc2afb6 33ce7467 8a03d8b9 1c6d8cb6 cf80892b 49833b9e "
		"cab5ab21 c98d217f 07d271d6 87722d5f 767f6c7a 3e0a5181 b3498108 c2010628 75444c4d 684d11bd 3519c3c3 fe5f3a9b 32e26305 "
		"05e4ae1d 9e041a74 2e5d6cbb 2ad01a12 ccc6037b 195c0143 d435b585 475b6472 673daf50 557ad826 4de8f324 b455abbb 4de8f324 "
		"557ad826 673daf50 475b6472 d435b585 195c0143 ccc6037b 2ad01a12 2e5d6cbb 9e041a74 05e4ae1d 32e26305 fe5f3a9b 3519c3c3 "
		"684d11bd 75444c4d c2010628 b3498108 3e0a5181 767f6c7a 87722d5f 07d271d6 6238da9f c365bd97 c59981e7 4afefd16 0f00e6c2 "
		"aa68b07d cecd0cc3 c4a23638 f0134cdc 04bc7af6 09a282eb 09a4e140*35 24ddaa35*3");
}

TEST_CASE("The title's Delete and Copy change the cards and read them again, as the original does", "[Content][Menu]")
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
	CheckMenuScenario(Scenario, "aeebfc50",
		"44223061 ef6008a9 854c0222*2 7c54d75c*2 8172c16c*2 538d6d19 e8e08344*32 236ffcab d1d0ecdc a2c3264b 69093596 be88e75a "
		"759d3cae c7cf8ab8 a2973eda 3dff566e 082cc4cc 7f81bc57 797ab439 1775df06 05042b96 33cb7284 1761dc16 6c2c9bd7 080db7ea "
		"2d7f21db d0d6bf53 4587172f 7a848692 68d67fb6 18032b4a bd4f7ada db9b141f d236e630 eb27b018 437ae240 6a8df147 d27d0ef1 "
		"6e736745 45e7be6e b50101ee 5c8b5c4e 62a9ba62 f655560f 1e197c3c*35 1a428c28 1e197c3c b8b14106*6 571de4a4 b8b14106 "
		"4a6b4268*6 0339157e 92ec2973 b9e96a54 90c870c1 deb9af6e cc887ede 06ca61f8 4fa8a0dd 948be029 2ffa98dd c065f59c 43e86294 "
		"811106e1 50945b19 439639ad 9bbd35b2 be369f3e cd73b1b3 e7a9c32e e7f87c5f 2fde1b05 e2836919 7a5e1a99 b80290d2 849989e1 "
		"337068b1 143f5f73 c9e7da34 0dd3a432 591733fc e8fb5cac bf8d01fb e8fb5cac 591733fc 0dd3a432 c9e7da34 143f5f73 337068b1 "
		"849989e1 b80290d2 7a5e1a99 e2836919 9b9dfd5d 1b4d1a40 c79de078 4d57d213 0d92ea2e 43367419 7c865eb3 01f790f1 09584da3 "
		"23710cd5 e09f8ae8 48f83d6d a0321b71 2978e0de a3ed0487 a6232f1f 846c33c5 d551c48d 883d51d4 182930c2 a9eebbd4 4eb89feb "
		"f696195e a6396962 b0cd7457 ccca58b4 f0af775e f745c880 3e404282 e409343c cd83d6bd e673435d c3d95a46 2e82e468 aab59d3b "
		"33f4a2a1 00e59c7a 3b92f009 c93baf11 73811253 248acbe9 8fa0012f 56ee55e6 2b4ed0ca 0a90a543 d9fa3471 4925c386 080f5606 "
		"8f210b87 fc8f3bf8 a5e53067 2cd42f8f 0b8261c1 ccf0d986 cee23ff2 c7c76569 df925830 f1b1c770 55201bc9 de5f9761 e07552b5 "
		"81bd61d0 88629e25 e4b4515c 7d365890 dbf9bba8 a53c1013 f3a68317 d8dbea0a 8572b0a3 d1a16745 7b5191a5 2d6dabca 094d31f3 "
		"9ece4b2a 036b3126 6059de7e 6672a7b7 5c5f2bed d11a82cc 4f617fe4 8db6ba42 5d2a4880 ebdf655b 686d00f7 c1f38daf 686d00f7 "
		"ebdf655b 5d2a4880 8db6ba42 4f617fe4 d11a82cc 5c5f2bed 6672a7b7 81b8cbe0 75545052 8881e96c 74e626c9 0952b956 ef35cede "
		"2c0abb5b 674abc2b 397993d7 4d57d213 55f499a3 29b9b88f 69814b5c bb1f12be e206db46 9b39b3a8 57c49927 eeeb14be b5399b9c "
		"0ca94d4f bd57449f c4198183 f8b94445 3b8b27db f8b94445 c4198183 bd57449f 0ca94d4f b5399b9c eeeb14be e8544329 f98a02db "
		"0633738b 759e4355 89721bb0 8b3d9bfc 639e55cc 76e6b61b 2bfefe66 3917ba73 28608411 cc3f48bd 4c574585 5ff00e84 34877470 "
		"0e168d30 7103dab1 3124e9e9 8a00c3fc 63bcc0a6 45923ff1 d09a2580 40cb3e09 320d7af8*39 f0e7658c 320d7af8 215c6452*6 "
		"07c51242 7fef5607 df477815 0d9f37e5 ebe789d2 3a836cd6 4850d348 24f987fb 1a3c19d3 6c9eee54 a13ef151 b8148eda 6ace62e7 "
		"5cd1bc2c 0e466033 9c5d3b0b 91664592 a8fb821c 52eca333 905dbdf3 52eca333 a8fb821c 91664592 9c5d3b0b 0e466033 5cd1bc2c "
		"6ace62e7 b8148eda a13ef151 6c9eee54 1a3c19d3 24f987fb 4850d348 3a836cd6 ebe789d2 0d9f37e5 627a2db9 853d0093 76bde070 "
		"d7a525f1 ef3edfbf d817ee35 b429297c 34b4f422 e611ae4c 00139ed3 d1c773e8 be3dda33 7b9b8f1d 7f8a8a80 5715b4f3 a5384ddf "
		"5715b4f3 7f8a8a80 7b9b8f1d be3dda33 9e53ea36 a8309dcd 43c6d776 9d6bb593 4702e783 26709204 28840245 ed117739 9d82bfc0 "
		"3378122d 1be80578 d30ae6df 6d90c05e 4e7d85b9 cfa29c3c e6934611 8e3920af b5ffbece 08f9985d 69efdcfc 6d7fe539 e7cd23bd "
		"f9debd3e 0fe391a9 e8492f67 7337cf74 2e1c5319 9fcf5bfc 2e1c5319 7337cf74 e8492f67 0fe391a9 e8ad5e7d 24b881c3 76f9300e "
		"a20a03ef a69e2a92 126a866b 13be08bf 32fd2ad4 376154f1 aa2653a2 579d8b32 5f75be66 70d810b3 b893eab8 965fff76 1894f087 "
		"b31a851a 1e7abf7a 5c044102 1ddf7c89 6965ede1 7a6c42f9 5a663614 d191d4ab 2767fdba b4a0e049 0ad8d199 661d1314 0ad8d199 "
		"b4a0e049 2767fdba d191d4ab 5a663614 7a6c42f9 6965ede1 1ddf7c89 5c044102 1e7abf7a a52b016c 21f28411 f1b962a6 65dd9b90 "
		"14fb2e8e 4058cd36 470f3cc5 06250a7d 695a0f8b 1e7c15e9 0cd6db48 5a29cf5c 83eec812 1ee95e10 57e9ed80 b8b14106*39 3ba120fc "
		"4bfa548e e314c931 774e1ff8 b9cbb7ae fff95e2c d4d27843 84e782a9 752985ee f8d9376d 1e9b71a2 e12b98b7 33f38d19 a784d790 "
		"f74052de befd9700 4676c0e3 d0a47f58 3b612937 d8d275f9 bc322f8c 191b4010 328c95e0 6c99bf0c c8c7a09e 4c77ba05 cfdff9d4 "
		"b8a1c7fa 7e1ec3d4 78ea3346 04af6ed1 548e371a 04af6ed1 78ea3346 7e1ec3d4 b8a1c7fa cfdff9d4 4c77ba05 c8c7a09e 6c99bf0c "
		"328c95e0 191b4010 d7663070 412f2c65 8a824b33 f92cef99 7341fc5b e383d4cb d8e38496 84bac96a 366b403c b0b22315 e66a4bc9 "
		"6cb80178 89d0de46 29ba1761 16abaacf 742564b4 659f24b6 d86097c6 3d413a5d 0c7faf88 6038e7ee 8eca9865 994a9b8b 993285eb "
		"1e9f85cc dfdd1e79 0f11d6f2 59946de7 5fcd0cd2 aa94f749 859b7526 72f9e29b ac9ef17f 3e23711c 254184f3 a802e259 292b19c6 "
		"f591e124 5933eec6 07f7b9e0 d8965a08 c60aad61 55a77855 ca8c6804 d544c147 0b82b8de b4f44008 4cd0f90a a829a639 3214df41 "
		"6434c86f 0cba660f f7907845 526b8a7d 27e59aef b37eaa76 30779f18 01250fc9 2b0d370c 80752b9e 86efa245 3ed8360c c75f1bfd "
		"3aa71862 1450a6d3 cccf466b 2380c5e1 5abf65c6 ded1f21b 406b1231 d4ea1796 c49ecc47 76b8df53 22917661 c515a18c 766a0abc "
		"1acf2bc0 44a7764f 98f339b0 59036cdb f048a988 cc5d1439 7ebc67e1 da9c0a03 62ff64ec 9a692242 62ff64ec da9c0a03 7ebc67e1 "
		"cc5d1439 f048a988 59036cdb 98f339b0 44a7764f 1acf2bc0 766a0abc baf6700b b0b22315 737b0743 b8b14106*39 5629d6aa 576d0e89 "
		"b3949f60*73");
}

TEST_CASE("The title's commands find no data on empty cards, as the original does", "[Content][Menu]")
{
	// Each command's notice, then out by Cancel.
	FMenuScenario Scenario{"file_title_empty", EMenuMode::LoadGame, 0,
		{{0, 40}, {Cross, 2}, {0, 70}, {Cross, 2}, {0, 40},
			{Cross, 2}, {0, 30}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 40}, {Cross, 2}, {0, 30}, {Down, 2}, {0, 6}, {Cross, 2}, {0, 40},
			{Cross, 2}, {0, 30}, {Circle, 2}, {0, 60}},
		400};
	Scenario.Cards = {{{}, {}}};
	CheckMenuScenario(Scenario, "7808f6f3",
		"44223061 ef6008a9 854c0222*2 7c54d75c*2 8172c16c*2 538d6d19 e8e08344*32 236ffcab d1d0ecdc a2c3264b 69093596 be88e75a "
		"759d3cae c7cf8ab8 a2973eda 3dff566e 082cc4cc 7f81bc57 797ab439 1775df06 05042b96 33cb7284 1761dc16 6c2c9bd7 080db7ea "
		"2d7f21db d0d6bf53 4587172f 7a848692 68d67fb6 18032b4a bd4f7ada db9b141f d236e630 eb27b018 437ae240 6a8df147 d27d0ef1 "
		"6e736745 45e7be6e b50101ee 5c8b5c4e 62a9ba62 84ee47d9 8fc6089b*35 9486ff36 e8bfbafd a65e2c38 338d59c6 038c4ec0 357b3028 "
		"3bd32ffa 9b751be1 2e9fe229 889e6c5e*33 568996c8 e8bfbafd c994cfe8 8fc6089b*29 ab91d8f5 8fc6089b 41eb50a3*6 3317333b "
		"357f3ed0 6e326b61 e3690d4f 84993022 9d29d250 75113ba0 f44af204 145b98fc cd10120c*33 6e1179f0 357f3ed0 69289dd6 "
		"41eb50a3*29 291870e1 41eb50a3 d1576f91*6 ff84df21 bd3cf482 2cadd12f 3b77c7f4 1778200d 848db834 bb200c90 a08a8833 "
		"dc73543b f4322957*33 dee1287b bd3cf482 03ee9abc d1576f91*29 fbf15b87 576d0e89 b3949f60*47");
}

TEST_CASE("The title's cards walk a full card and refuse a copy to it, as the original does", "[Content][Menu]")
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
	CheckMenuScenario(Scenario, "c090f3cf",
		"44223061 ef6008a9 854c0222*2 7c54d75c*2 8172c16c*2 538d6d19 e8e08344*32 236ffcab d1d0ecdc a2c3264b 69093596 be88e75a "
		"759d3cae c7cf8ab8 a2973eda 3dff566e 082cc4cc 7f81bc57 797ab439 1775df06 05042b96 33cb7284 1761dc16 6c2c9bd7 080db7ea "
		"2d7f21db d0d6bf53 4587172f 7a848692 68d67fb6 18032b4a bd4f7ada db9b141f d236e630 eb27b018 437ae240 6a8df147 d27d0ef1 "
		"6e736745 45e7be6e b50101ee 5c8b5c4e 62a9ba62 9e6b8c0d 47b31817*35 ab0cade1 1219fa5e 8d4c447d f4b73767 14d87d3c 9ac53a48 "
		"22568be7 905bc2b1 cae368de b880d151 9cf3395c 2bee1a47 7f05c64a f4870670 a0adb48b b8a446e2 a0adb48b f4870670 7f05c64a "
		"2bee1a47 9cf3395c b880d151 cae368de 905bc2b1 22568be7 9ac53a48 14d87d3c f4b73767 ade8fc28 d2ddb86c 2492da41 9de8a986 "
		"a88037cb 8772eaa8 90f380d3 9c52ec8e b1ef9e1b ff46d332 52c92ccf 237023ca a9d6ddd0 67b74f87 b8782888 6f158984 441f478d "
		"3be1931f 78a3173a 10cb1d3b 78a3173a 3be1931f 441f478d a71cecab ee1e6e73 597497d2 07d03a28 491d129f 7a70ff38 170dc880 "
		"a1023354 e399ce35 c8c67597 5d93625f ae415bc7 d2cdf8d7 9dfc92e2 184632fd 9432a0bd cc5b8635 79d32bf3 fbdf721b 4949f9eb "
		"81fb0abd a20b7e09 eaaace0c 9135cc5a 6af73b94 4e835c02 70dc4157 c76f1882 f54b4d36 c76f1882 70dc4157 526ba0bb 756e087a "
		"bf28ee1d 857d054c 47f0bf2d 47bc0a40 a4161ee1 5e02652d 7218725a 2f5b3096 96ce44f2 8407c24b 9dd0a046 6705f949 5b9920f7 "
		"e1d2fd03 1833b7fa e56e7816 d63ff3ec 077062f1 8a3e4047 dfef0528 4255ab14 009cf941 0dddedad c0809dea b30cf748 402cb32e "
		"c0e501d4 532f11c0 05e19bde 4b40bf6e 98f1bbee 0fc0c306 b11c798b fd1c4135 52f02f3c dcd25846 a1c1048d 55bfbc45 dddae36c "
		"be353d9a be04e5d4 08e8c76b b022cefc 3793bb1b e711accc 15c800e7 a06a0b70 cd1d4689 b5579de1 e1c65d2e 3b397b2b 118b771b "
		"3abbcf6b bb49a87a 3d28ad00 f533b77e 3379b4f0 ecf71992 a70bee4a 4d38b60f 2a852ad2 9caea8ec bebfb036 4aa3a44c 9606f555 "
		"a1e6e93d 0ad92715 f9fbcf42 66d7c497 4a2b1c2b acd4f42f 52226f8e 0fb98c9f 6092fa0d e1cfe772 85d13e08 4bb7ccfb 4894e979 "
		"9887242a 6e1ced06 53a9ee66 4d12865c 33309e19 4316dc07 294d7cd0 d76417f5 526e9ef7 c0c6722c 7ba952fa 7b87e9d1 7d251c6e "
		"bc095047 7d251c6e 1f1f2001 c2c1ccbb 6e73c65b 5de3c60f 6f372c5e b0e6241f 4c25a4a7 e70d73a2 d6916d79 79db487a 4d5ecad9 "
		"c5b86824 859c159e 13bd2c5f 96e9d32b ea35f9b5 6ce01971 9197c8b9 b568f9c2 b16d12ee 5de461c1 783ce0d8 cc59f853 8329f0c4 "
		"a72f89a5 4ddea952 34fb36cc f69b3ed3 e7267274 4b341f6a 5e0303b6 4b341f6a e7267274 f69b3ed3 34fb36cc 4ddea952 a72f89a5 "
		"7b23b8cf 705282d2 042b041f 78eaf3f8 32f9c850 c1f58a63 5e88c682 c93f59d8 317d7479 07550088 60b8769a 2feb3cbb 929a0e38 "
		"8c13baca 37a2f4ef 2768dfb6 3a4be334 bce60268 9d95a687 6f372c5e 5de3c60f 6e73c65b c2c1ccbb 1f1f2001 7d251c6e bc095047 "
		"7d251c6e 1f1f2001 c2c1ccbb 6e73c65b 5de3c60f 6f372c5e 8461e2c2 2fc4cd8e 47b31817*30 6b185137 47b31817 b0b08163*6 "
		"7edd01c2 7b8adbe2 0aed919d 4106509a df5fa97c d6651df4 9c39cabd f2b2e1d3 aee5c53d dda0b66d f1dee224 4185528e 2020e5be "
		"881ebd1a c44b7db5 d129adc5 3a1a7923 ac9fd982 3a1a7923 d129adc5 c44b7db5 881ebd1a 2020e5be 4185528e f1dee224 dda0b66d "
		"aee5c53d f2b2e1d3 9c39cabd d6651df4 df5fa97c 4106509a 7ea57bb2 1c42c02a 5c51820e 9ad00547 f38a8102 cacaa14a 5b0a4d3b "
		"d5135ffc 2d56f281 b9af833e 56993c9d ed460c02 7ce83335 fafaf3e4 4be3ad68 505cef7d c250e662 5500bda2 b42580b7 05480223 "
		"c1620017 2adcbfec d2ca584a c1ebd4a0 3df95843 8aeb23d5 0d635174 3ecf342e cc4a25cb 3c3579d9 c853a585 f19f405c 612b5eaf "
		"1071bfb5 9e12a7c1 553e3515 41f37bfc 6d4d2a5c 59398960 4e923fe9 25bb6f35 af95cb7a a2496be6 fce2e665 22ceeee6 c671ed4f "
		"16f5f87e 005c486d 3f1bdbed ca1e87bc d303bc01 d129adc5 6a4ba0fb b028336f b126d2f5 6f94a3b3 d0b69aa6 7a79b6aa 0a7cc188 "
		"fc8ebad8 5397c2f1 8af3801f 00edda44 0edd5822 b9d4f293 c9f406af c963f4dd ea6bf545 34e22ab3 a1df82e9 7be99387 cebced72 "
		"4c350594 ebc775c3 cc6ed7c3 0a883139 94db4eb4 de71b565 11cb50a1 2c129c0c 54496419 4ccca08b 54496419 2c129c0c 11cb50a1 "
		"de71b565 94db4eb4 0a883139 cc6ed7c3 ebc775c3 4c350594 cebced72 e94a0a5b aedafc9c 7a62cde9 b0b08163*39 5c330399 576d0e89 "
		"b3949f60*73");
}

TEST_CASE("The Normal Menu saves into a free cell and closes the File screen, as the original does", "[Content][Menu]")
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
	CheckMenuScenario(Scenario, "116e1764",
		"b77d9678 ef6008a9 be3773cb*2 9747faed*2 55f515d3*2 fb6d7173*2 50e08734*2 ef0726b1*2 89009a2f*2 80f248a1 462d4224 "
		"ff029088 a7da7abb 70cb63bf b94ab69a 55c72d8b 58743d52 e3f30649 95fcaca1 1e8199e5 2d9782e3 0468dfcd a4587a25 92dae811 "
		"4a75632f cc8ed20f 30e5fa33 8906bd54 5a3b84db 54839eb7 d00b2233 28a8a972 a2f1ad4b*7 3ec2891e 7659ef61 940de2ff c092e65b "
		"1c5ec3ed 9fcccd01 f31effcb 0788d4cf fafa8d27 7215c089 c125e2e8 8840c53e 8ab3d990 b1b04b8b 056dd375 037d94b6 d8508e6a "
		"ae8d9435 72b50bb0 39bf5147 5f5ff9e3 ef45d93c 8c630faf 703e9b27 48bd679c 108ed06c e7910e72 b5987a88 ef0783b9 d43f6baa "
		"c2a99134 f38bfb9a 32132201 a50caabc 0a2e3d27 ee702602 f6afb8d1 9a0b24d5*75 11092321 ca323ae1 3809c807 e2c468df 9140d4d3 "
		"3c5f0905 08a29d2b acf15876 04aa9d51 a2003240 a30598ac c39574d6 30985812 ae8286f0 ad7ca18d f32092c1 89dc730e f2969b7b "
		"615ec657 ca17f087 28718ce8 f911bbbf 8c7599d7 8316a4ea ff459313 e7db3927 8d6a807c ecfce889 a1508391 1c7382ed 24626ef9 "
		"0770e4c0 1c5f0656 b0c1ded4 0ee46ab3 b0c1ded4 1c5f0656 0770e4c0 24626ef9 1c7382ed a1508391 ecfce889 b00a8b1e bfe041a7 "
		"60ab709f 80d5f64c 5c0dabe5 2070b647 d7ae15fa 46b7b4b1 2ff34ebd f69da5e4 d73f81da 998f9d64 ffe0e0b5 b31c1dd2 085e91fa "
		"4940b615 9377c7ce d6c19d53 6b69ea0a 4b8529a0 a5bb9b57 1d73f107 f585ac43 df9be055 1c1b271f 8ce23283 c5dd4e2c 70e9ff22 "
		"8a7f967e 42de4d75 fccbf9be 2cc9d08b 6adf114f 1cc261d0 04aa593d 7d1029b5 d9957deb d65b0630 a8126d2d c18e4f6b e16f791d "
		"5591ce00 c2f880bb eae9b739 ba09305b e8fdae82 78e786a1 863f4126 68bf7933 43e83cbd 3ca1814f d30dca03 59398f16 6193bbd5 "
		"74f774bc 70f815a8 952297ef 70f815a8 2db0c0cd aef47573 b859c875 15afed32 63bd88f6 207d2096 81fafe83 6b9dc40d 6b412b70 "
		"f0f8d7ae dee891fd 133a92c9 b1ea9f10 6a5552fa 36eff082 36d66d9f 9c5bb7e1 38beae24 8b2b06c1 95601a04 89bf036c 3f7fa623 "
		"0a83076b a9e92621 9440a14c d3136b3e 96d2f7ca 09ccaa5c ad55c33d 3e6316b6 9e94f3fa 3e6316b6 ad55c33d 09ccaa5c 96d2f7ca "
		"d3136b3e 9440a14c a9e92621 0a83076b 3f7fa623 89bf036c 95601a04 8b2b06c1 38beae24 9c5bb7e1 36d66d9f 36eff082 0a4fdb1c "
		"b153314f 19a9a169 cc5c383d cc95350f e8f676bd 4ca42ca9 20392974 6c4bbff3 c19075ed 54c9a694 c4e3b0ee b6d20b98 bef0c5fb "
		"04708952 4d1e4b0e 1f9438e3 0ae30666 279fa073 940e6a62 f8f2953a 8d818662 205f070b 9162c354*2 3b568f2a*60 6a49bc50*60 "
		"806bbc34*60 b5c8347a*10");
}

TEST_CASE("The Normal Menu saves over a save in its slot, as the original does", "[Content][Menu]")
{
	// Overwrite previous file? Yes: No.1 written again.
	FMenuScenario Scenario{"file_normal_overwrite", EMenuMode::Normal, 1,
		{{0, 45}, {Cross, 2}, {0, 110}, {Cross, 2}, {0, 40},
			{Cross, 2}, {0, 30}, {Left, 2}, {0, 6}, {Cross, 2}, {0, 40}, {Cross, 2}, {0, 100}},
		480};
	Scenario.PlayTime = 0x13579;
	Scenario.Cards = {{{FirstSave(), ThirdSave()}, {}}};
	CheckMenuScenario(Scenario, "d982713a",
		"0cd12bd3 ef6008a9 be3773cb*2 9747faed*2 55f515d3*2 fb6d7173*2 50e08734*2 ef0726b1*2 89009a2f*2 61063503 d27c2f4e "
		"1f1805ef 4c89d363 8a1d2501 0267fb36 e25c6b51 1090cb74 5c70056f de367a9a d49f1f91 435d2c11 86f8ac12 e012d66d 9dbf5ac0 "
		"a33373d1 8f86fd20 a5fe68f1 0b92b3c6 93045b6e 465bf2ca 6a1fe250 1d0b3346 f7b2552b*7 4607413f 0869f51c 22b9d225 6fc190bf "
		"ef26f957 e1debbc5 9d2144b9 61fba970 15399c68 f29871d8 2f11f023 8fae8593 d6506cab 4973b3b3 706b125a c6d1d13b f8ee8df6 "
		"e1f104a9 b0d5df3e 63752c66 e9ee5ca8 41bdc198 08cabe80 0cf09490 6c2f5c3e d7680927 feb414d1 a29ab386 ddf501ed f56bde59 "
		"9c95c4f9 e3d008d8 afc53035 1710dbae a61b32c7 74fb55ed c3ee2725 ee614acb*75 5bf111f2 5e9a3ef6 5aba8e7f 053fd77d 3d65667d "
		"aaf120a5 c125042b 532f79ba d9e73ef4 fb142cca 31249d28 46422e4a f8287002 58852521 729465ba cce2ee17 8dc14bdc 6e87eacc "
		"ddaf6fd2 36fd2625 539eab1e 2c1c797c 98bd55bb b46c2628 2b492217 93b3d355 840590cd 726170b3 c36b51bc 8bcc20fc c1439c05 "
		"ab3017ce f5f3be6c f9c349aa 094f1da0 f9c349aa f5f3be6c ab3017ce c1439c05 8bcc20fc c36b51bc 726170b3 2bc86e23 7f71c9cf "
		"9a845c17 31d842be 04432142 416934aa cb29ed4c caaaaa24 d936f0f3 a315065d 7a3107da 557cd5f8 96622e1e 08f30dfd 7ff7a863 "
		"25e5dc61 e7c5cc2f 8322d23e 8f52a65e 0f486dd9 a96c8dc6 5b7d6e9a 296593a2 aac40f9c 0b7e0dc9 aac40f9c 296593a2 5b7d6e9a "
		"a96c8dc6 0f486dd9 8f52a65e 8322d23e 05015247 e151f8d1 ee0c1261 ec8fcefb 75085e2c ccd96b28 e144b2a3 2bf25393 b847990b "
		"36fd2625 28b85df5 c6176bf8 0614e89b 2031ee68 f0b6fceb c543ce5e 28b57154 3ca61885 2f6eaa0e 2fbc05b2 9a2f364c 78f48a61 "
		"671d4392 6e6c3843 80bc8f60 6e6c3843 671d4392 78f48a61 9a2f364c 2fbc05b2 2f6eaa0e 3ca61885 340e8be2 063e5809 82677ae4 "
		"fdffad39 f428f37d 5a9f17e0 3e02cbc6 44dc6517 8e3fadd4 bc10a4c5 a9548039 c8ed3a0f 627ef666 e64f40e9 14381513 8c610141 "
		"631eb315 583c59ec b9b8bc7a 7ee79c61 e239289d 0e743fd8 540ad439 bea6fa96 9ba8e78c 5461c00a 08694ffe d258421a 7c1d1d05 "
		"ff91e53b 1fa0e881 0dff7784 18865729 3fddf4ec 7a4ac32b 62e6dc37 78c5c78c b11d79fb 473b7709 d03a4679 f22272e9 b28dbe90 "
		"ab43bbc0*29 075a75a9*60 b3461458*60 8a1e5e36*25");
}

TEST_CASE("The Normal Menu deletes a save where saving is not allowed, as the original does", "[Content][Menu]")
{
	// Copy and Delete only; No.3 deleted.
	FMenuScenario Scenario{"file_normal_delete", EMenuMode::Normal, 0,
		{{0, 45}, {Cross, 2}, {0, 110}, {Down, 2}, {0, 6},
			{Cross, 2}, {0, 40}, {Down, 2}, {0, 20}, {Cross, 2}, {0, 30}, {Left, 2}, {0, 6}, {Cross, 2}, {0, 30}, {Left, 2}, {0, 6},
			{Cross, 2}, {0, 40}, {Cross, 2}, {0, 100}},
		520};
	Scenario.LastCommand = 1;
	Scenario.Cards = {{{FirstSave(), ThirdSave()}, {LongSave()}}};
	CheckMenuScenario(Scenario, "2d9c9069",
		"0cd12bd3 ef6008a9 be3773cb*2 9747faed*2 55f515d3*2 fb6d7173*2 50e08734*2 ef0726b1*2 89009a2f*2 61063503 d27c2f4e "
		"1f1805ef 4c89d363 8a1d2501 0267fb36 e25c6b51 1090cb74 5c70056f de367a9a d49f1f91 435d2c11 86f8ac12 e012d66d 9dbf5ac0 "
		"a33373d1 8f86fd20 a5fe68f1 0b92b3c6 93045b6e 465bf2ca a12b58e6 8b8d13b3 5840e9c7*7 0cc0a5ba d6e7b347 28290a05 db377d7b "
		"97252d48 b6375831 42739600 dd030d3c 476a6e9a 9d50ab45 0975eb89 36c7a090 1a5629d2 41c51e5c 9b3d803b 9b012b61 4fbcc99f "
		"12658605 87213c45 d9a211f3 a003d602 b2995c25 85ca7a92 de81f7a3 57641811 e8a3cfda e49de392 42a90bf5 4da22353 d047cbf2 "
		"94420ad9 bb9e4511 9af473bb f899881b 6c58ec28 7c46fa59 1c5e5c53 643b7ca0*75 5a40fdd9 643b7ca0 f7c96466*6 a2099b35 "
		"255c9748 a6cd25bd 3afb7f81 2123aad2 98fa4935 a4bff675 956f1d2f 52c22fa7 26e52409 76c5ac46 61f54fe6 305a5eb0 7ead571b "
		"f01001a7 fdb81e5f 525c9cec 362dfc7b 57705a40 59ef896d 20f7805c dced3dea a9f40839 30281b9f 8af9ea40 5b56f42b 8139268f "
		"5b56f42b 8af9ea40 30281b9f a9f40839 dced3dea 20f7805c 59ef896d 57705a40 362dfc7b 525c9cec fdb81e5f f01001a7 7ead571b "
		"305a5eb0 61f54fe6 6d45d304 e0a22bf2 f7add7a2 be88f26a 55b5e1ad 472f807e f149b20c f90ba7db ac1f4b55 2975bc21 71ce0dff "
		"0b093b3f c0bb3a08 2ff1b338 7cfd920e dbbd9153 1d7ed307 dbbd9153 7cfd920e 2ff1b338 c0bb3a08 0b093b3f 858a9249 a3b2eab8 "
		"c0f65894 7db18f5a eb0a3652 8ae92d8f 43b38277 e5db4220 afe2b1dc 19c55f8c 8468ea13 fe05fd8c bc17bf66 6652133d b7a0a5c9 "
		"d40c2ea8 bbc47856 32550afb 9a299c72 12f1ccfc f07da8d1 a8802bae 95de9275 85b503ad c247508c 8222e7b0 5fb260ea 8222e7b0 "
		"c247508c 85b503ad 95de9275 a8802bae 0a6cc3da fe230e9e 82627883 dd82f9da 1631d48f 6fea979b 3ab83137 0d2f85c0 bcb91924 "
		"3692b2b4 09484ef0 01796813 ad9efd6e 90826a9a f708f067 6a5fa39d be6a1d73 6f8767a6 a01668cc ec2374cd 04fd2f43 d4020d15 "
		"3654e6be 5273c9d5 0a7f1d8c 18820063 1473995a 18820063 0a7f1d8c 5273c9d5 3654e6be d4020d15 04fd2f43 ec2374cd a01668cc "
		"69be0a5d 0e246079 907be06d 725e5b28 2a789d59 60f39c05 0684726d 9a87c0f5 ba8e912c 966845b3 7f651070 97318b2a 6169e320 "
		"684dd26b a83eddd8 e57bab9c f80f09ed f97fdb3c 44396b5d 0ef14836 49ad6cbb 0f122907 77850240 91ba5ac7 77850240 59d4c103 "
		"16393cd1 4c1d496e e40d222d f8d05edf 3eb1cd32 0903c772 7a02f34c 2ee5e20d 00591ea1 2da0cf83 1ad3f10f eb380c3a a1aaf088 "
		"b3b245fd 4000aaed 7c02c322 6e32b2d9 09dc91b1 dc1c6bda 8fc6d430 ff6dba14 1181e84a f624f97a 155eaf81 ee6f3a2d adb8acea "
		"4cd046df de9d1a77 0c79fbaf 09708807 93419d54 06856c4b 77286c87 42c46e80 fdaa265a 4d988674 cf9f6dd7 db32f22b da3d927c "
		"6b488b37 2621d62f 546fcd0e 38f5f960 9c28cd30 0a45669a 6ae0b7cc 6ff2de02 00781172 287df5be fc2e8841 abcc3d81 e77ca276 "
		"b8742116 3c79959b*44 13b59011*60 4847f3ba*40");
}

TEST_CASE("The disc change saves as disc 2 and stays for another save, as the original does", "[Content][Menu]")
{
	// Save? Yes; a free cell of the first card; the grid read again stays; Cancel goes on to disc 2.
	FMenuScenario Scenario{"file_disc_change_save", EMenuMode::DiscChange, 0,
		{{0, 50}, {Cross, 2}, {0, 90}, {Cross, 2}, {0, 40},
			{Right, 2}, {0, 10}, {Down, 2}, {0, 20}, {Cross, 2}, {0, 30}, {Left, 2}, {0, 6}, {Cross, 2}, {0, 40}, {Cross, 2}, {0, 40},
			{Circle, 2}, {0, 100}},
		520};
	Scenario.Disc = 2;
	Scenario.Cards = {{{FirstSave()}, {LongSave()}}};
	CheckMenuScenario(Scenario, "8e40499a",
		"ac8ab4a0 09a4e140*33 a355032a 116d7d6e 2f1dcc45 eaf6f244 3659b4fa f20f6c1d 0c9ca275 8eb6f094 87ff3b70*9 d1e05fae "
		"09a4e140 a135240b 9cf0e9bf a55ec1c6 5960c27a 119ded06 6c491941 5b0b6d00 bedc3bc0 b0e629a1 01472414 2cbd9517 85311459 "
		"c286461e a265b0e8 ff390c4d cd5976a5 1bb64830 8cbed9cd 4195df20 654405f4 21c6a514 7ccf0cb2 c9dccfdb a174a507 375c3a82 "
		"91de519e 89f1916b 144abddf c4e1c641 3d6cd732 bddce282 175b0819 4d4997fa 15aed947 682860d8 b14e9a8c ae1451ce*54 879a9b9d "
		"1ba5227d 2e308b01 e486b307 82157285 bad11d63 75b46a38 608f2d44 f79184fd f9ae3b3b 9e1f4387 04cd162e a86621b7 7149795c "
		"e8dfc95b 3dfafef1 86319d38 7d306fe7 86319d38 3dfafef1 e8dfc95b 7149795c a86621b7 04cd162e 9e1f4387 f9ae3b3b f79184fd "
		"608f2d44 75b46a38 bad11d63 82157285 e486b307 2487b0f8 dce44c43 9fe6553f e969dd54 7267f722 3e6be70b eca401b7 b90365f6 "
		"80097c2e bc5729ed e1962fe8 61f8efb9 e7947aab c4ce6395 cd074f7f bfd48a31 daf4f921 48eeaff5 daf4f921 bfd48a31 cd074f7f "
		"c4ce6395 931ead65 c0f24d75 48c2329e 0528f35f 6edf2015 c8209660 6630eedc 36b79cd3 ba41d134 3e4855b6 3774068e 85f05883 "
		"b8ef901b fc661c06 6d8f08c0 5cf8e9aa 08acdbd7 bb31c74c 394f3516 05dca866 f0b01725 7a473413 bc0097a9 1f426d7e bade4e55 "
		"b54adf4f ffa05ec3 ba046491 5ea7ce68 2588e017 2d9a9220 9a40cb34 39fac202 894fbb02 88826e67 3de2fb20 53b9c4ab 697a7e4f "
		"9e49f719 4195cd55 636b60df 920bbf9e 3c07afe2 726aea3b fba9be6c da7c969f f571ac7b ec69c27c b2703079 522f7c05 068d563e "
		"5a21275b 16ec3d84 7742e4ae 89b567f0 d6e6fceb a884a05f 9c197699 465593bb b6e810c6 465593bb 9c197699 6fffff68 3ab5c878 "
		"ef45e6cc 3b87b79a de7471dc 0c469794 c67cbaee 370b543f 32484f34 0c052182 524ca406 5493ae7e dd533758 c7e8d98b 7e97b9ba "
		"c46ec20b eaae614f 8086a0b0 deef89e0 f85ea400 30d796dd b11a0d28 3eb547b8 7b8628d8 e7af6c7d 868193cc f3a28cec 4c14efec "
		"3a1136f2 b24c49e1 3a1136f2 4c14efec f3a28cec 868193cc e7af6c7d 7b8628d8 3eb547b8 b11a0d28 30d796dd f85ea400 deef89e0 "
		"8086a0b0 eaae614f 97a432c2 dfb2bc83 113f8308 a6f1e38c 3a53b0b6 97e5d015 67076956 b2e68bde 65785071 b3a9bb85 360d1c7d "
		"814a1389 cc128995 2faa1154 5437ce7d 8b9a47e4 a0e329d0 6e77a6a0 34b57d2a 6e77a6a0 a0e329d0 8b9a47e4 5437ce7d 2faa1154 "
		"cc128995 814a1389 360d1c7d b3a9bb85 65785071 b2e68bde 67076956 97e5d015 3a53b0b6 a6f1e38c f111e273 613df803 df23cc40 "
		"3e20a9eb 2d948a3b 26e61079 2f213d97 5e014ec6 de46dbe6 09a4e140*35 24ddaa35*2");
}
