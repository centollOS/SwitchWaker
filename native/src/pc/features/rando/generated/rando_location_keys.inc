// GENERATED FILE - do not edit. Regenerate with: python3 -I native/tools/rando/gen_location_keys.py
// Source: wwrando 1.10.0_9775811 (MIT), commit 9775811b6fd039992822fbaf30ea6259b339f4b0: logic/item_locations.txt
//   sha1 32b1a68bee57ac46358b5374de2485661d61bc60, data/actor_parameters.txt classes;
//   disc GZLE01 (offsets, actor names and item ids only), files read sha1 b071aca890ae6b2f3a51f8b4e3fd3daf8f01dfe1
// Generator: native/tools/rando/gen_location_keys.py sha1 7e62e7a3f5a97eb1729bfa77a91fafc452672edc
// Included once, by pc_rando_lockeys.cpp (inside namespace pcr::gen).
// 320 locations, 511 keys (key i is loc_paths()[i]); classes: CHEST 166, SCOB 57, ACTR 14, ACTR+CODE 7, EVENT 10, EVENT+CODE 1, CODE 58, CODE+CUSTOM 1, CUSTOM 2, ADDED+CODE 4.
// Fingerprints checked on the disc: 391 dzx items, 11 event actions, 77 code bytes; 0 mismatches.

static const LocKey kLocKeys[] = {
    // LinkUG/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_OUTSET_ISLAND_UNDERNEATH_LINKS_HOUSE, 0x06, KEY_FP_ITEM, "LinkUG", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x32, 0x00}, 0x06, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // Omasao/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_OUTSET_ISLAND_MESA_THE_GRASSCUTTERS_HOUSE, 0x04, KEY_FP_ITEM, "Omasao", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x32, 0x00}, 0x04, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // rels/d_a_npc_ji1.rel@4267
    {KeyKind::Code, PCR_LOC_OUTSET_ISLAND_ORCA_GIVE_10_KNIGHTS_CRESTS, 0xAA, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0xAA, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_ji1.rel", 0x00004267u, PCR_ADDED_NONE},
    // Ojhous/Stage.arc/Event00D:Ji1_TeachSpRollCut/Actor003/Action003
    {KeyKind::Event, PCR_LOC_OUTSET_ISLAND_ORCA_GIVE_10_KNIGHTS_CRESTS, 0xAA, KEY_FP_ITEM, "Ojhous", "Stage", nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0, "Ji1_TeachSpRollCut", 13, 3, 3, 284, 143, "046pget", 170, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // rels/d_a_npc_ji1.rel@42F7
    {KeyKind::Code, PCR_LOC_OUTSET_ISLAND_ORCA_HIT_500_TIMES, 0x07, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_ji1.rel", 0x000042F7u, PCR_ADDED_NONE},
    // rels/d_a_bigelf.rel@21C7
    {KeyKind::Code, PCR_LOC_OUTSET_ISLAND_GREAT_FAIRY, 0xAC, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0xAC, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_bigelf.rel", 0x000021C7u, PCR_ADDED_NONE},
    // main.dol@8035BD39
    {KeyKind::Code, PCR_LOC_OUTSET_ISLAND_JABUNS_CAVE, 0x69, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x69, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::MainDol, "main.dol", 0x8035BD39u, PCR_ADDED_NONE},
    // Pjavdou/Room0.arc/Chest000
    {KeyKind::Added, PCR_LOC_OUTSET_ISLAND_JABUNS_CAVE, 0x69, 0, "Pjavdou", "Room0", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {}, 0, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_JABUN_CHEST},
    // sea/Room44.arc/ScalableObject03A
    {KeyKind::Dzx, PCR_LOC_OUTSET_ISLAND_DIG_UP_BLACK_SOIL, 0x07, KEY_FP_ITEM, "sea", "Room44", "SCOB", 58, DzxField::TagKbItem, 0x08, 4, 0x000000FFu, {0x54, 0x61, 0x67, 0x4B, 0x62, 0x00, 0x00, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // Cave09/Room20.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_OUTSET_ISLAND_SAVAGE_LABYRINTH_FLOOR_30, 0xF9, KEY_FP_ITEM, "Cave09", "Room20", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x43, 0x54, 0x66, 0x00, 0x00}, 0xF9, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // Cave10/Room20.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_OUTSET_ISLAND_SAVAGE_LABYRINTH_FLOOR_50, 0x07, KEY_FP_ITEM, "Cave10", "Room20", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x33, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // rels/d_a_npc_tc.rel@6117
    {KeyKind::Code, PCR_LOC_WINDFALL_ISLAND_JAIL_TINGLE_FIRST_GIFT, 0x21, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x21, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_tc.rel", 0x00006117u, PCR_ADDED_NONE},
    // rels/d_a_npc_tc.rel@611F
    {KeyKind::Code, PCR_LOC_WINDFALL_ISLAND_JAIL_TINGLE_SECOND_GIFT, 0xDC, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0xDC, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_tc.rel", 0x0000611Fu, PCR_ADDED_NONE},
    // Pnezumi/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_WINDFALL_ISLAND_JAIL_MAZE_CHEST, 0x23, KEY_FP_ITEM, "Pnezumi", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x00, 0x00}, 0x23, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // CustomSymbol:doc_bandam_green_potion_slot_item_id
    {KeyKind::Code, PCR_LOC_WINDFALL_ISLAND_CHU_JELLY_JUICE_SHOP_GIVE_15_GREEN_CHU_JELLY, 0x52, 0, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x00, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::CustomSymbol, "doc_bandam_green_potion_slot_item_id", 0x00000000u, PCR_ADDED_NONE},
    // CustomSymbol:doc_bandam_blue_potion_slot_item_id
    {KeyKind::Code, PCR_LOC_WINDFALL_ISLAND_CHU_JELLY_JUICE_SHOP_GIVE_15_BLUE_CHU_JELLY, 0x53, 0, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x00, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::CustomSymbol, "doc_bandam_blue_potion_slot_item_id", 0x00000000u, PCR_ADDED_NONE},
    // rels/d_a_npc_mk.rel@20CB
    {KeyKind::Code, PCR_LOC_WINDFALL_ISLAND_IVAN_CATCH_KILLER_BEES, 0x07, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_mk.rel", 0x000020CBu, PCR_ADDED_NONE},
    // rels/d_a_npc_ho.rel@0A07
    {KeyKind::Code, PCR_LOC_WINDFALL_ISLAND_MRS_MARIE_CATCH_KILLER_BEES, 0x05, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x05, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_ho.rel", 0x00000A07u, PCR_ADDED_NONE},
    // rels/d_a_npc_ho.rel@0B4B
    {KeyKind::Code, PCR_LOC_WINDFALL_ISLAND_MRS_MARIE_GIVE_1_JOY_PENDANT, 0x04, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x04, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_ho.rel", 0x00000B4Bu, PCR_ADDED_NONE},
    // rels/d_a_npc_ho.rel@0B77
    {KeyKind::Code, PCR_LOC_WINDFALL_ISLAND_MRS_MARIE_GIVE_21_JOY_PENDANTS, 0x9C, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x9C, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_ho.rel", 0x00000B77u, PCR_ADDED_NONE},
    // rels/d_a_npc_ho.rel@0C87
    {KeyKind::Code, PCR_LOC_WINDFALL_ISLAND_MRS_MARIE_GIVE_40_JOY_PENDANTS, 0x43, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x43, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_ho.rel", 0x00000C87u, PCR_ADDED_NONE},
    // Ocmera/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_WINDFALL_ISLAND_LENZOS_HOUSE_LEFT_CHEST, 0xE6, KEY_FP_ITEM, "Ocmera", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x00, 0x00}, 0xE6, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // Ocmera/Stage.arc/Chest001
    {KeyKind::Dzx, PCR_LOC_WINDFALL_ISLAND_LENZOS_HOUSE_RIGHT_CHEST, 0x05, KEY_FP_ITEM, "Ocmera", "Stage", "TRES", 1, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x32, 0x00}, 0x05, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // rels/d_a_npc_photo.rel@1C13
    {KeyKind::Code, PCR_LOC_WINDFALL_ISLAND_LENZOS_HOUSE_BECOME_LENZOS_ASSISTANT, 0x1F, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x1F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_photo.rel", 0x00001C13u, PCR_ADDED_NONE},
    // rels/d_a_npc_photo.rel@1C37
    {KeyKind::Code, PCR_LOC_WINDFALL_ISLAND_LENZOS_HOUSE_BRING_FOREST_FIREFLY, 0x26, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x26, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_photo.rel", 0x00001C37u, PCR_ADDED_NONE},
    // rels/d_a_npc_photo.rel@3BE3
    {KeyKind::Code, PCR_LOC_WINDFALL_ISLAND_LENZOS_HOUSE_BRING_FOREST_FIREFLY, 0x26, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x26, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_photo.rel", 0x00003BE3u, PCR_ADDED_NONE},
    // Orichh/Room0.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_WINDFALL_ISLAND_HOUSE_OF_WEALTH_CHEST, 0x04, KEY_FP_ITEM, "Orichh", "Room0", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x32, 0x00}, 0x04, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // Orichh/Stage.arc/Event04C:Gp1_Get_Itm/Actor002/Action001
    {KeyKind::Event, PCR_LOC_WINDFALL_ISLAND_MAGGIES_FATHER_GIVE_20_SKULL_NECKLACES, 0xEE, KEY_FP_ITEM, "Orichh", "Stage", nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0, "Gp1_Get_Itm", 76, 2, 1, 901, 336, "011get_item", 238, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // rels/d_a_npc_kp1.rel@17D7
    {KeyKind::Code, PCR_LOC_WINDFALL_ISLAND_MAGGIE_FREE_ITEM, 0x9A, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x9A, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_kp1.rel", 0x000017D7u, PCR_ADDED_NONE},
    // rels/d_a_npc_kp1.rel@1817
    {KeyKind::Code, PCR_LOC_WINDFALL_ISLAND_MAGGIE_DELIVERY_REWARD, 0x07, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_kp1.rel", 0x00001817u, PCR_ADDED_NONE},
    // Opub/Stage.arc/Event00E:Get_Mo3_Ltr/Actor002/Action001
    {KeyKind::Event, PCR_LOC_WINDFALL_ISLAND_CAFE_BAR_POSTMAN, 0x9B, KEY_FP_ITEM, "Opub", "Stage", nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0, "Get_Mo3_Ltr", 14, 2, 1, 101, 27, "011get_item", 155, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // rels/d_a_npc_people.rel@C55B
    {KeyKind::Code, PCR_LOC_WINDFALL_ISLAND_KREEB_LIGHT_UP_LIGHTHOUSE, 0x07, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_people.rel", 0x0000C55Bu, PCR_ADDED_NONE},
    // sea/Room11.arc/Layer1/Chest000
    {KeyKind::Dzx, PCR_LOC_WINDFALL_ISLAND_TRANSPARENT_CHEST, 0x07, KEY_FP_ITEM, "sea", "Room11", "TRE1", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x42, 0x4D, 0x73, 0x00, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room11.arc/Layer3/Chest000
    {KeyKind::Dzx, PCR_LOC_WINDFALL_ISLAND_TRANSPARENT_CHEST, 0x07, KEY_FP_ITEM, "sea", "Room11", "TRE3", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x42, 0x4D, 0x73, 0x00, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room11.arc/Layer5/Chest000
    {KeyKind::Dzx, PCR_LOC_WINDFALL_ISLAND_TRANSPARENT_CHEST, 0x07, KEY_FP_ITEM, "sea", "Room11", "TRE5", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x42, 0x4D, 0x73, 0x00, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Stage.arc/Event0E3:TACT_TT11/Actor001/Action003
    {KeyKind::Event, PCR_LOC_WINDFALL_ISLAND_TOTT_TEACH_RHYTHM, 0x72, KEY_FP_ITEM, "sea", "Stage", nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0, "TACT_TT11", 227, 1, 3, 3141, 1704, "059get_dance", 5, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // Asoko/Stage.arc/Layer2/Chest000
    {KeyKind::Dzx, PCR_LOC_WINDFALL_ISLAND_PIRATE_SHIP, 0x31, KEY_FP_ITEM, "Asoko", "Stage", "TRE2", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x33, 0x00}, 0x31, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // Asoko/Stage.arc/Layer3/Chest000
    {KeyKind::Dzx, PCR_LOC_WINDFALL_ISLAND_PIRATE_SHIP, 0x31, KEY_FP_ITEM, "Asoko", "Stage", "TRE3", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x33, 0x00}, 0x31, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // rels/d_a_auction.rel@4491
    {KeyKind::Code, PCR_LOC_WINDFALL_ISLAND_5_RUPEE_AUCTION, 0xE4, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0xE4, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_auction.rel", 0x00004491u, PCR_ADDED_NONE},
    // rels/d_a_auction.rel@4489
    {KeyKind::Code, PCR_LOC_WINDFALL_ISLAND_40_RUPEE_AUCTION, 0x1F, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x1F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_auction.rel", 0x00004489u, PCR_ADDED_NONE},
    // rels/d_a_auction.rel@4499
    {KeyKind::Code, PCR_LOC_WINDFALL_ISLAND_60_RUPEE_AUCTION, 0xED, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0xED, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_auction.rel", 0x00004499u, PCR_ADDED_NONE},
    // rels/d_a_auction.rel@44A1
    {KeyKind::Code, PCR_LOC_WINDFALL_ISLAND_80_RUPEE_AUCTION, 0x07, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_auction.rel", 0x000044A1u, PCR_ADDED_NONE},
    // rels/d_a_npc_rsh1.rel@3117
    {KeyKind::Code, PCR_LOC_WINDFALL_ISLAND_ZUNARI_STOCK_EXOTIC_FLOWER_IN_ZUNARIS_SHOP, 0x2A, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x2A, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_rsh1.rel", 0x00003117u, PCR_ADDED_NONE},
    // rels/d_a_npc_rsh1.rel@338F
    {KeyKind::Code, PCR_LOC_WINDFALL_ISLAND_ZUNARI_STOCK_EXOTIC_FLOWER_IN_ZUNARIS_SHOP, 0x2A, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x2A, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_rsh1.rel", 0x0000338Fu, PCR_ADDED_NONE},
    // CustomSymbol:zunari_magic_armor_slot_item_id
    {KeyKind::Code, PCR_LOC_WINDFALL_ISLAND_ZUNARI_STOCK_EXOTIC_FLOWER_IN_ZUNARIS_SHOP, 0x2A, 0, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x00, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::CustomSymbol, "zunari_magic_armor_slot_item_id", 0x00000000u, PCR_ADDED_NONE},
    // rels/d_a_npc_people.rel@C54B
    {KeyKind::Code, PCR_LOC_WINDFALL_ISLAND_SAM_DECORATE_THE_TOWN, 0x07, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_people.rel", 0x0000C54Bu, PCR_ADDED_NONE},
    // rels/d_a_npc_people.rel@C21B
    {KeyKind::Code, PCR_LOC_WINDFALL_ISLAND_KANE_PLACE_SHOP_GURU_STATUE_ON_GATE, 0x05, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x05, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_people.rel", 0x0000C21Bu, PCR_ADDED_NONE},
    // rels/d_a_npc_people.rel@C21F
    {KeyKind::Code, PCR_LOC_WINDFALL_ISLAND_KANE_PLACE_POSTMAN_STATUE_ON_GATE, 0x05, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x05, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_people.rel", 0x0000C21Fu, PCR_ADDED_NONE},
    // rels/d_a_npc_people.rel@C223
    {KeyKind::Code, PCR_LOC_WINDFALL_ISLAND_KANE_PLACE_SIX_FLAGS_ON_GATE, 0x04, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x04, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_people.rel", 0x0000C223u, PCR_ADDED_NONE},
    // rels/d_a_npc_people.rel@C227
    {KeyKind::Code, PCR_LOC_WINDFALL_ISLAND_KANE_PLACE_SIX_IDOLS_ON_GATE, 0x04, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x04, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_people.rel", 0x0000C227u, PCR_ADDED_NONE},
    // sea/Stage.arc/Event084:get_empty_btl/Actor002/Action001
    {KeyKind::Event, PCR_LOC_WINDFALL_ISLAND_MILA_FOLLOW_THE_THIEF, 0x50, KEY_FP_ITEM, "sea", "Stage", nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0, "get_empty_btl", 132, 2, 1, 1325, 636, "011get_item", 80, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // rels/d_a_npc_kg1.rel@2C28
    {KeyKind::Code, PCR_LOC_WINDFALL_ISLAND_BATTLESQUID_FIRST_PRIZE, 0x07, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_kg1.rel", 0x00002C28u, PCR_ADDED_NONE},
    // rels/d_a_npc_kg1.rel@2C29
    {KeyKind::Code, PCR_LOC_WINDFALL_ISLAND_BATTLESQUID_SECOND_PRIZE, 0xCC, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0xCC, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_kg1.rel", 0x00002C29u, PCR_ADDED_NONE},
    // rels/d_a_npc_kg1.rel@2C2C
    {KeyKind::Code, PCR_LOC_WINDFALL_ISLAND_BATTLESQUID_UNDER_20_SHOTS_PRIZE, 0xF1, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0xF1, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_kg1.rel", 0x00002C2Cu, PCR_ADDED_NONE},
    // rels/d_a_npc_people.rel@C54F
    {KeyKind::Code, PCR_LOC_WINDFALL_ISLAND_POMPIE_AND_VERA_SECRET_MEETING_PHOTO, 0xEB, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0xEB, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_people.rel", 0x0000C54Fu, PCR_ADDED_NONE},
    // rels/d_a_npc_people.rel@C55F
    {KeyKind::Code, PCR_LOC_WINDFALL_ISLAND_KAMO_FULL_MOON_PHOTO, 0xF0, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0xF0, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_people.rel", 0x0000C55Fu, PCR_ADDED_NONE},
    // rels/d_a_npc_people.rel@C553
    {KeyKind::Code, PCR_LOC_WINDFALL_ISLAND_MINENCO_MISS_WINDFALL_PHOTO, 0xEF, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0xEF, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_people.rel", 0x0000C553u, PCR_ADDED_NONE},
    // rels/d_a_npc_people.rel@C557
    {KeyKind::Code, PCR_LOC_WINDFALL_ISLAND_LINDA_AND_ANTON, 0x07, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_people.rel", 0x0000C557u, PCR_ADDED_NONE},
    // sea/Stage.arc/Event0D8:TACT_HT/Actor001/Action005
    {KeyKind::Event, PCR_LOC_DRAGON_ROOST_ISLAND_WIND_SHRINE, 0x6D, KEY_FP_ITEM, "sea", "Stage", nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0, "TACT_HT", 216, 1, 5, 3008, 1577, "059get_dance", 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // Atorizk/Stage.arc/Event000:Get_Rupee/Actor002/Action001
    {KeyKind::Event, PCR_LOC_DRAGON_ROOST_ISLAND_RITO_AERIE_GIVE_HOSKIT_20_GOLDEN_FEATHERS, 0x06, KEY_FP_ITEM, "Atorizk", "Stage", nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0, "Get_Rupee", 0, 2, 1, 4, 0, "011get_item", 6, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room13.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_DRAGON_ROOST_ISLAND_CHEST_ON_TOP_OF_BOULDER, 0x0F, KEY_FP_ITEM, "sea", "Room13", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x49, 0x6B, 0x00, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room13.arc/Chest001
    {KeyKind::Dzx, PCR_LOC_DRAGON_ROOST_ISLAND_FLY_ACROSS_PLATFORMS_AROUND_ISLAND, 0x06, KEY_FP_ITEM, "sea", "Room13", "TRES", 1, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x32, 0x00}, 0x06, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // Atorizk/Stage.arc/Event003:GETMOTHERLETTER/Actor000/Action000
    {KeyKind::Event, PCR_LOC_DRAGON_ROOST_ISLAND_RITO_AERIE_MAIL_SORTING, 0x99, KEY_FP_ITEM, "Atorizk", "Stage", nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0, "GETMOTHERLETTER", 3, 0, 0, 14, 4, "011_get_item", 153, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // TF_06/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_DRAGON_ROOST_ISLAND_SECRET_CAVE, 0x05, KEY_FP_ITEM, "TF_06", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x32, 0x00}, 0x05, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // M_NewD2/Stage.arc/Chest005
    {KeyKind::Dzx, PCR_LOC_DRAGON_ROOST_CAVERN_FIRST_ROOM, 0x15, KEY_FP_ITEM, "M_NewD2", "Stage", "TRES", 5, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x53, 0x77, 0x00, 0x00}, 0x15, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // M_NewD2/Stage.arc/Chest008
    {KeyKind::Dzx, PCR_LOC_DRAGON_ROOST_CAVERN_ALCOVE_WITH_WATER_JUGS, 0x4C, KEY_FP_ITEM, "M_NewD2", "Stage", "TRES", 8, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x00, 0x00}, 0x4C, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // M_NewD2/Room13.arc/Actor005
    {KeyKind::Dzx, PCR_LOC_DRAGON_ROOST_CAVERN_WATER_JUG_ON_UPPER_SHELF, 0x1F, 0, "M_NewD2", "Room13", "ACTR", 5, DzxField::TsuboItem, 0x1C, 2, 0x0000FF00u, {0x4B, 0x6D, 0x74, 0x75, 0x62, 0x00, 0x00, 0x00}, 0x00, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // M_NewD2/Stage.arc/Chest006
    {KeyKind::Dzx, PCR_LOC_DRAGON_ROOST_CAVERN_BOARDED_UP_CHEST, 0x15, KEY_FP_ITEM, "M_NewD2", "Stage", "TRES", 6, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x32, 0x00}, 0x15, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // M_NewD2/Stage.arc/Chest00A
    {KeyKind::Dzx, PCR_LOC_DRAGON_ROOST_CAVERN_CHEST_ACROSS_LAVA_PIT, 0xF6, KEY_FP_ITEM, "M_NewD2", "Stage", "TRES", 10, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x00, 0x00}, 0xF6, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // M_NewD2/Stage.arc/Chest007
    {KeyKind::Dzx, PCR_LOC_DRAGON_ROOST_CAVERN_RAT_ROOM, 0x4D, KEY_FP_ITEM, "M_NewD2", "Stage", "TRES", 7, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x00, 0x00}, 0x4D, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // M_NewD2/Stage.arc/Chest00B
    {KeyKind::Dzx, PCR_LOC_DRAGON_ROOST_CAVERN_RAT_ROOM_BOARDED_UP_CHEST, 0x15, KEY_FP_ITEM, "M_NewD2", "Stage", "TRES", 11, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x32, 0x00}, 0x15, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // M_NewD2/Room3.arc/Actor016
    {KeyKind::Dzx, PCR_LOC_DRAGON_ROOST_CAVERN_BIRDS_NEST, 0x15, KEY_FP_ITEM, "M_NewD2", "Room3", "ACTR", 22, DzxField::ItemItem, 0x08, 4, 0x000000FFu, {0x69, 0x74, 0x65, 0x6D, 0x00, 0x00, 0x00, 0x00}, 0x15, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // M_NewD2/Stage.arc/Chest004
    {KeyKind::Dzx, PCR_LOC_DRAGON_ROOST_CAVERN_DARK_ROOM, 0x1F, KEY_FP_ITEM, "M_NewD2", "Stage", "TRES", 4, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x32, 0x00}, 0x1F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // M_NewD2/Stage.arc/Chest00D
    {KeyKind::Dzx, PCR_LOC_DRAGON_ROOST_CAVERN_TINGLE_CHEST_IN_HUB_ROOM, 0x03, KEY_FP_ITEM, "M_NewD2", "Stage", "TRES", 13, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x53, 0x77, 0x00, 0x00}, 0x03, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // M_NewD2/Room6.arc/Actor023
    {KeyKind::Dzx, PCR_LOC_DRAGON_ROOST_CAVERN_POT_ON_UPPER_SHELF_IN_POT_ROOM, 0x1F, 0, "M_NewD2", "Room6", "ACTR", 35, DzxField::TsuboItem, 0x1C, 2, 0x0000FF00u, {0x6F, 0x6F, 0x74, 0x75, 0x62, 0x6F, 0x31, 0x00}, 0x00, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // M_NewD2/Stage.arc/Chest003
    {KeyKind::Dzx, PCR_LOC_DRAGON_ROOST_CAVERN_POT_ROOM_CHEST, 0xEC, KEY_FP_ITEM, "M_NewD2", "Stage", "TRES", 3, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x35, 0x00}, 0xEC, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // rels/d_a_npc_md.rel@AF2F
    {KeyKind::Code, PCR_LOC_DRAGON_ROOST_CAVERN_MINIBOSS, 0x25, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x25, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_md.rel", 0x0000AF2Fu, PCR_ADDED_NONE},
    // M_Dra09/Stage.arc/Chest000
    {KeyKind::Added, PCR_LOC_DRAGON_ROOST_CAVERN_MINIBOSS, 0x25, 0, "M_Dra09", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {}, 0, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_MEDLI_GRAPPLING_HOOK_CHEST},
    // M_NewD2/Stage.arc/Chest009
    {KeyKind::Dzx, PCR_LOC_DRAGON_ROOST_CAVERN_UNDER_ROPE_BRIDGE, 0x1F, KEY_FP_ITEM, "M_NewD2", "Stage", "TRES", 9, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x53, 0x77, 0x00, 0x00}, 0x1F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // M_NewD2/Stage.arc/Chest00C
    {KeyKind::Dzx, PCR_LOC_DRAGON_ROOST_CAVERN_TINGLE_STATUE_CHEST, 0xA3, KEY_FP_ITEM, "M_NewD2", "Stage", "TRES", 12, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x36, 0x00}, 0xA3, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // M_NewD2/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_DRAGON_ROOST_CAVERN_BIG_KEY_CHEST, 0x4E, KEY_FP_ITEM, "M_NewD2", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x34, 0x00}, 0x4E, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // M_NewD2/Stage.arc/Chest001
    {KeyKind::Dzx, PCR_LOC_DRAGON_ROOST_CAVERN_BOSS_STAIRS_RIGHT_CHEST, 0x48, KEY_FP_ITEM, "M_NewD2", "Stage", "TRES", 1, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x32, 0x00}, 0x48, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // M_NewD2/Stage.arc/Chest002
    {KeyKind::Dzx, PCR_LOC_DRAGON_ROOST_CAVERN_BOSS_STAIRS_LEFT_CHEST, 0x03, KEY_FP_ITEM, "M_NewD2", "Stage", "TRES", 2, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x32, 0x00}, 0x03, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // M_NewD2/Room10.arc/Actor002
    {KeyKind::Dzx, PCR_LOC_DRAGON_ROOST_CAVERN_BOSS_STAIRS_RIGHT_POT, 0x1F, 0, "M_NewD2", "Room10", "ACTR", 2, DzxField::TsuboItem, 0x1C, 2, 0x0000FF00u, {0x6F, 0x6F, 0x74, 0x75, 0x62, 0x6F, 0x31, 0x00}, 0x00, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // rels/d_a_btd.rel@36FE
    {KeyKind::Code, PCR_LOC_DRAGON_ROOST_CAVERN_GOHMA_HEART_CONTAINER, 0x08, 0, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x00, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_btd.rel", 0x000036FEu, PCR_ADDED_NONE},
    // M_DragB/Room0.arc/Actor015
    {KeyKind::Dzx, PCR_LOC_DRAGON_ROOST_CAVERN_GOHMA_HEART_CONTAINER, 0x08, 0, "M_DragB", "Room0", "ACTR", 21, DzxField::BossItem, 0x08, 4, 0x0000FF00u, {0x42, 0x69, 0x74, 0x65, 0x6D, 0x00, 0x00, 0x00}, 0x00, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // rels/d_a_deku_item.rel@0DC3
    {KeyKind::Code, PCR_LOC_FOREST_HAVEN_ON_TREE_BRANCH, 0x34, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x34, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_deku_item.rel", 0x00000DC3u, PCR_ADDED_NONE},
    // Omori/Room0.arc/Layer0/Actor000
    {KeyKind::Dzx, PCR_LOC_FOREST_HAVEN_ON_TREE_BRANCH, 0x34, 0, "Omori", "Room0", "ACT0", 0, DzxField::DekuItem, 0x08, 4, 0x000000FFu, {0x69, 0x74, 0x65, 0x6D, 0x44, 0x65, 0x6B, 0x00}, 0x02, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // Omori/Room0.arc/Layer1/Actor00B
    {KeyKind::Dzx, PCR_LOC_FOREST_HAVEN_ON_TREE_BRANCH, 0x34, 0, "Omori", "Room0", "ACT1", 11, DzxField::DekuItem, 0x08, 4, 0x000000FFu, {0x69, 0x74, 0x65, 0x6D, 0x44, 0x65, 0x6B, 0x00}, 0x02, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room41.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_FOREST_HAVEN_SMALL_ISLAND_CHEST, 0xE0, KEY_FP_ITEM, "sea", "Room41", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x00, 0x00}, 0xE0, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // kindan/Stage.arc/Chest002
    {KeyKind::Dzx, PCR_LOC_FORBIDDEN_WOODS_FIRST_ROOM, 0x4C, KEY_FP_ITEM, "kindan", "Stage", "TRES", 2, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x00, 0x00}, 0x4C, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // kindan/Stage.arc/Chest001
    {KeyKind::Dzx, PCR_LOC_FORBIDDEN_WOODS_INSIDE_HOLLOW_TREES_MOUTH, 0x48, KEY_FP_ITEM, "kindan", "Stage", "TRES", 1, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x4B, 0x64, 0x00, 0x00}, 0x48, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // kindan/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_FORBIDDEN_WOODS_CLIMB_TO_TOP_USING_BOKO_BABA_BULBS, 0xE7, KEY_FP_ITEM, "kindan", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x4B, 0x00}, 0xE7, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // kindan/Room1.arc/Actor06B
    {KeyKind::Dzx, PCR_LOC_FORBIDDEN_WOODS_POT_HIGH_ABOVE_HOLLOW_TREE, 0x1F, 0, "kindan", "Room1", "ACTR", 107, DzxField::TsuboItem, 0x1C, 2, 0x0000FF00u, {0x6F, 0x6F, 0x74, 0x75, 0x62, 0x6F, 0x31, 0x00}, 0x00, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // kindan/Stage.arc/Chest004
    {KeyKind::Dzx, PCR_LOC_FORBIDDEN_WOODS_HOLE_IN_TREE, 0x04, KEY_FP_ITEM, "kindan", "Stage", "TRES", 4, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x32, 0x00}, 0x04, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // kindan/Stage.arc/Chest00C
    {KeyKind::Dzx, PCR_LOC_FORBIDDEN_WOODS_MORTH_PIT, 0x03, KEY_FP_ITEM, "kindan", "Stage", "TRES", 12, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x32, 0x00}, 0x03, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // kindan/Stage.arc/Chest006
    {KeyKind::Dzx, PCR_LOC_FORBIDDEN_WOODS_VINE_MAZE_LEFT_CHEST, 0x4D, KEY_FP_ITEM, "kindan", "Stage", "TRES", 6, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x00, 0x00}, 0x4D, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // kindan/Stage.arc/Chest003
    {KeyKind::Dzx, PCR_LOC_FORBIDDEN_WOODS_VINE_MAZE_RIGHT_CHEST, 0x15, KEY_FP_ITEM, "kindan", "Stage", "TRES", 3, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x32, 0x00}, 0x15, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // kindan/Room7.arc/Actor0E0
    {KeyKind::Dzx, PCR_LOC_FORBIDDEN_WOODS_HIGHEST_POT_IN_VINE_MAZE, 0x1F, 0, "kindan", "Room7", "ACTR", 224, DzxField::TsuboItem, 0x1C, 2, 0x0000FF00u, {0x6F, 0x6F, 0x74, 0x75, 0x62, 0x6F, 0x31, 0x00}, 0x00, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // kindan/Stage.arc/Chest009
    {KeyKind::Dzx, PCR_LOC_FORBIDDEN_WOODS_TALL_ROOM_BEFORE_MINIBOSS, 0x1F, KEY_FP_ITEM, "kindan", "Stage", "TRES", 9, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x32, 0x00}, 0x1F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // kinMB/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_FORBIDDEN_WOODS_MOTHULA_MINIBOSS_ROOM, 0x2D, KEY_FP_ITEM, "kinMB", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x33, 0x00}, 0x2D, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // kindan/Stage.arc/Chest00B
    {KeyKind::Dzx, PCR_LOC_FORBIDDEN_WOODS_PAST_SEEDS_HANGING_BY_VINES, 0x1F, KEY_FP_ITEM, "kindan", "Stage", "TRES", 11, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x32, 0x00}, 0x1F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // kindan/Stage.arc/Chest00D
    {KeyKind::Dzx, PCR_LOC_FORBIDDEN_WOODS_CHEST_ACROSS_RED_HANGING_FLOWER, 0x03, KEY_FP_ITEM, "kindan", "Stage", "TRES", 13, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x32, 0x00}, 0x03, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // kindan/Stage.arc/Chest00E
    {KeyKind::Dzx, PCR_LOC_FORBIDDEN_WOODS_TINGLE_STATUE_CHEST, 0xA4, KEY_FP_ITEM, "kindan", "Stage", "TRES", 14, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x36, 0x00}, 0xA4, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // kindan/Stage.arc/Chest007
    {KeyKind::Dzx, PCR_LOC_FORBIDDEN_WOODS_CHEST_IN_LOCKED_TREE_TRUNK, 0xF5, KEY_FP_ITEM, "kindan", "Stage", "TRES", 7, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x4B, 0x00}, 0xF5, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // kindan/Stage.arc/Chest008
    {KeyKind::Dzx, PCR_LOC_FORBIDDEN_WOODS_BIG_KEY_CHEST, 0x4E, KEY_FP_ITEM, "kindan", "Stage", "TRES", 8, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x34, 0x00}, 0x4E, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // kindan/Stage.arc/Chest00A
    {KeyKind::Dzx, PCR_LOC_FORBIDDEN_WOODS_DOUBLE_MOTHULA_ROOM, 0x1F, KEY_FP_ITEM, "kindan", "Stage", "TRES", 10, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x53, 0x77, 0x00, 0x00}, 0x1F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // rels/d_a_bmd.rel@2B2E
    {KeyKind::Code, PCR_LOC_FORBIDDEN_WOODS_KALLE_DEMOS_HEART_CONTAINER, 0x08, 0, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x00, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_bmd.rel", 0x00002B2Eu, PCR_ADDED_NONE},
    // kinBOSS/Room0.arc/Actor002
    {KeyKind::Dzx, PCR_LOC_FORBIDDEN_WOODS_KALLE_DEMOS_HEART_CONTAINER, 0x08, 0, "kinBOSS", "Room0", "ACTR", 2, DzxField::BossItem, 0x08, 4, 0x0000FF00u, {0x42, 0x69, 0x74, 0x65, 0x6D, 0x00, 0x00, 0x00}, 0x00, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room23.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_GREATFISH_ISLE_HIDDEN_CHEST, 0x07, KEY_FP_ITEM, "sea", "Room23", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x33, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // Siren/Stage.arc/Chest001
    {KeyKind::Dzx, PCR_LOC_TOWER_OF_THE_GODS_CHEST_BEHIND_BOMBABLE_WALLS, 0x4C, KEY_FP_ITEM, "Siren", "Stage", "TRES", 1, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x00, 0x00}, 0x4C, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // Siren/Room3.arc/Actor004
    {KeyKind::Dzx, PCR_LOC_TOWER_OF_THE_GODS_POT_BEHIND_BOMBABLE_WALLS, 0x1F, 0, "Siren", "Room3", "ACTR", 4, DzxField::TsuboItem, 0x1C, 2, 0x0000FF00u, {0x6B, 0x6F, 0x74, 0x75, 0x62, 0x6F, 0x00, 0x00}, 0x00, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // Siren/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_TOWER_OF_THE_GODS_HOP_ACROSS_FLOATING_BOXES, 0x15, KEY_FP_ITEM, "Siren", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x53, 0x77, 0x00, 0x00}, 0x15, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // Siren/Stage.arc/Chest005
    {KeyKind::Dzx, PCR_LOC_TOWER_OF_THE_GODS_LIGHT_TWO_TORCHES, 0x1F, KEY_FP_ITEM, "Siren", "Stage", "TRES", 5, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x53, 0x77, 0x00, 0x00}, 0x1F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // Siren/Stage.arc/Chest002
    {KeyKind::Dzx, PCR_LOC_TOWER_OF_THE_GODS_SKULLS_ROOM_CHEST, 0x4D, KEY_FP_ITEM, "Siren", "Stage", "TRES", 2, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x00, 0x00}, 0x4D, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // Siren/Stage.arc/Chest006
    {KeyKind::Dzx, PCR_LOC_TOWER_OF_THE_GODS_SHOOT_EYE_ABOVE_SKULLS_ROOM_CHEST, 0xEA, KEY_FP_ITEM, "Siren", "Stage", "TRES", 6, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x00, 0x00}, 0xEA, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // Siren/Stage.arc/Chest00A
    {KeyKind::Dzx, PCR_LOC_TOWER_OF_THE_GODS_TINGLE_STATUE_CHEST, 0xA5, KEY_FP_ITEM, "Siren", "Stage", "TRES", 10, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x36, 0x00}, 0xA5, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // Siren/Stage.arc/Chest004
    {KeyKind::Dzx, PCR_LOC_TOWER_OF_THE_GODS_FIRST_CHEST_GUARDED_BY_ARMOS_KNIGHTS, 0x1F, KEY_FP_ITEM, "Siren", "Stage", "TRES", 4, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x47, 0x63, 0x00, 0x00}, 0x1F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // Siren/Stage.arc/Event060:hsehi1_tact/Actor003/Action004
    {KeyKind::Event, PCR_LOC_TOWER_OF_THE_GODS_STONE_TABLET, 0x6F, KEY_FP_ITEM, "Siren", "Stage", nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0, "hsehi1_tact", 96, 3, 4, 957, 355, "059get_dance", 2, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // SirenMB/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_TOWER_OF_THE_GODS_DARKNUT_MINIBOSS_ROOM, 0x27, KEY_FP_ITEM, "SirenMB", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x38, 0x00}, 0x27, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // Siren/Stage.arc/Chest007
    {KeyKind::Dzx, PCR_LOC_TOWER_OF_THE_GODS_SECOND_CHEST_GUARDED_BY_ARMOS_KNIGHTS, 0xF4, KEY_FP_ITEM, "Siren", "Stage", "TRES", 7, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x43, 0x54, 0x66, 0x00, 0x00}, 0xF4, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // Siren/Stage.arc/Chest008
    {KeyKind::Dzx, PCR_LOC_TOWER_OF_THE_GODS_FLOATING_PLATFORMS_ROOM, 0x15, KEY_FP_ITEM, "Siren", "Stage", "TRES", 8, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x32, 0x00}, 0x15, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // Siren/Stage.arc/Chest009
    {KeyKind::Dzx, PCR_LOC_TOWER_OF_THE_GODS_TOP_OF_FLOATING_PLATFORMS_ROOM, 0x1F, KEY_FP_ITEM, "Siren", "Stage", "TRES", 9, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x53, 0x77, 0x00, 0x00}, 0x1F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // Siren/Room17.arc/Actor006
    {KeyKind::Dzx, PCR_LOC_TOWER_OF_THE_GODS_EASTERN_POT_IN_BIG_KEY_CHEST_ROOM, 0x1F, 0, "Siren", "Room17", "ACTR", 6, DzxField::TsuboItem, 0x1C, 2, 0x0000FF00u, {0x6B, 0x6F, 0x74, 0x75, 0x62, 0x6F, 0x00, 0x00}, 0x00, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // Siren/Stage.arc/Chest00B
    {KeyKind::Dzx, PCR_LOC_TOWER_OF_THE_GODS_BIG_KEY_CHEST, 0x4E, KEY_FP_ITEM, "Siren", "Stage", "TRES", 11, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x34, 0x00}, 0x4E, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // rels/d_a_bst.rel@9D43
    {KeyKind::Code, PCR_LOC_TOWER_OF_THE_GODS_GOHDAN_HEART_CONTAINER, 0x08, 0, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x00, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_bst.rel", 0x00009D43u, PCR_ADDED_NONE},
    // SirenB/Room0.arc/Actor003
    {KeyKind::Dzx, PCR_LOC_TOWER_OF_THE_GODS_GOHDAN_HEART_CONTAINER, 0x08, 0, "SirenB", "Room0", "ACTR", 3, DzxField::BossItem, 0x08, 4, 0x0000FF00u, {0x42, 0x69, 0x74, 0x65, 0x6D, 0x00, 0x00, 0x00}, 0x00, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // main.dol@8035BD34
    {KeyKind::Code, PCR_LOC_HYRULE_MASTER_SWORD_CHAMBER, 0x39, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x39, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::MainDol, "main.dol", 0x8035BD34u, PCR_ADDED_NONE},
    // kenroom/Room0.arc/Chest000
    {KeyKind::Added, PCR_LOC_HYRULE_MASTER_SWORD_CHAMBER, 0x39, 0, "kenroom", "Room0", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {}, 0, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_MASTER_SWORD_CHEST},
    // sea/Room1.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_FORSAKEN_FORTRESS_PHANTOM_GANON, 0x33, KEY_FP_ITEM, "sea", "Room1", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x36, 0x00}, 0x33, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // majroom/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_FORSAKEN_FORTRESS_CHEST_OUTSIDE_UPPER_JAIL_CELL, 0x4C, KEY_FP_ITEM, "majroom", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x00, 0x00}, 0x4C, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // ma2room/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_FORSAKEN_FORTRESS_CHEST_OUTSIDE_UPPER_JAIL_CELL, 0x4C, KEY_FP_ITEM, "ma2room", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x00, 0x00}, 0x4C, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // ma3room/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_FORSAKEN_FORTRESS_CHEST_OUTSIDE_UPPER_JAIL_CELL, 0x4C, KEY_FP_ITEM, "ma3room", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x00, 0x00}, 0x4C, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // majroom/Stage.arc/Chest003
    {KeyKind::Dzx, PCR_LOC_FORSAKEN_FORTRESS_CHEST_INSIDE_LOWER_JAIL_CELL, 0x07, KEY_FP_ITEM, "majroom", "Stage", "TRES", 3, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x33, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // ma2room/Stage.arc/Chest003
    {KeyKind::Dzx, PCR_LOC_FORSAKEN_FORTRESS_CHEST_INSIDE_LOWER_JAIL_CELL, 0x07, KEY_FP_ITEM, "ma2room", "Stage", "TRES", 3, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x33, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // ma3room/Stage.arc/Chest003
    {KeyKind::Dzx, PCR_LOC_FORSAKEN_FORTRESS_CHEST_INSIDE_LOWER_JAIL_CELL, 0x07, KEY_FP_ITEM, "ma3room", "Stage", "TRES", 3, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x33, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // majroom/Stage.arc/Chest002
    {KeyKind::Dzx, PCR_LOC_FORSAKEN_FORTRESS_CHEST_GUARDED_BY_BOKOBLIN, 0x4D, KEY_FP_ITEM, "majroom", "Stage", "TRES", 2, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x00, 0x00}, 0x4D, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // ma2room/Stage.arc/Chest002
    {KeyKind::Dzx, PCR_LOC_FORSAKEN_FORTRESS_CHEST_GUARDED_BY_BOKOBLIN, 0x4D, KEY_FP_ITEM, "ma2room", "Stage", "TRES", 2, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x00, 0x00}, 0x4D, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // ma3room/Stage.arc/Chest002
    {KeyKind::Dzx, PCR_LOC_FORSAKEN_FORTRESS_CHEST_GUARDED_BY_BOKOBLIN, 0x4D, KEY_FP_ITEM, "ma3room", "Stage", "TRES", 2, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x00, 0x00}, 0x4D, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // majroom/Stage.arc/Chest001
    {KeyKind::Dzx, PCR_LOC_FORSAKEN_FORTRESS_CHEST_ON_BED, 0x03, KEY_FP_ITEM, "majroom", "Stage", "TRES", 1, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x32, 0x00}, 0x03, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // ma2room/Stage.arc/Chest001
    {KeyKind::Dzx, PCR_LOC_FORSAKEN_FORTRESS_CHEST_ON_BED, 0x03, KEY_FP_ITEM, "ma2room", "Stage", "TRES", 1, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x32, 0x00}, 0x03, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // ma3room/Stage.arc/Chest001
    {KeyKind::Dzx, PCR_LOC_FORSAKEN_FORTRESS_CHEST_ON_BED, 0x03, KEY_FP_ITEM, "ma3room", "Stage", "TRES", 1, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x32, 0x00}, 0x03, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // rels/d_a_bdk.rel@5CCE
    {KeyKind::Code, PCR_LOC_FORSAKEN_FORTRESS_HELMAROC_KING_HEART_CONTAINER, 0x08, 0, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x00, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_bdk.rel", 0x00005CCEu, PCR_ADDED_NONE},
    // M2tower/Room0.arc/Layer3/Actor01B
    {KeyKind::Dzx, PCR_LOC_FORSAKEN_FORTRESS_HELMAROC_KING_HEART_CONTAINER, 0x08, 0, "M2tower", "Room0", "ACT3", 27, DzxField::BossItem, 0x08, 4, 0x0000FF00u, {0x42, 0x69, 0x74, 0x65, 0x6D, 0x00, 0x00, 0x00}, 0x00, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room1.arc/Layer2/Actor001
    {KeyKind::Dzx, PCR_LOC_FORSAKEN_FORTRESS_HELMAROC_KING_HEART_CONTAINER, 0x08, 0, "sea", "Room1", "ACT2", 1, DzxField::BossItem, 0x08, 4, 0x0000FF00u, {0x42, 0x69, 0x74, 0x65, 0x6D, 0x00, 0x00, 0x00}, 0x00, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room1.arc/Layer3/Actor001
    {KeyKind::Dzx, PCR_LOC_FORSAKEN_FORTRESS_HELMAROC_KING_HEART_CONTAINER, 0x08, 0, "sea", "Room1", "ACT3", 1, DzxField::BossItem, 0x08, 4, 0x0000FF00u, {0x42, 0x69, 0x74, 0x65, 0x6D, 0x00, 0x00, 0x00}, 0x00, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // main.dol@8035BD3C
    {KeyKind::Code, PCR_LOC_MOTHER_AND_CHILD_ISLES_INSIDE_MOTHER_ISLE, 0x35, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x35, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::MainDol, "main.dol", 0x8035BD3Cu, PCR_ADDED_NONE},
    // sea/Room9.arc/Chest000
    {KeyKind::Added, PCR_LOC_MOTHER_AND_CHILD_ISLES_INSIDE_MOTHER_ISLE, 0x35, 0, "sea", "Room9", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {}, 0, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_QUEEN_FAIRY_CHEST},
    // MiniKaz/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_FIRE_MOUNTAIN_CAVE_CHEST, 0x28, KEY_FP_ITEM, "MiniKaz", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x36, 0x00}, 0x28, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room20.arc/Chest001
    {KeyKind::Dzx, PCR_LOC_FIRE_MOUNTAIN_LOOKOUT_PLATFORM_CHEST, 0x03, KEY_FP_ITEM, "sea", "Room20", "TRES", 1, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x4F, 0x63, 0x00, 0x00}, 0x03, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room20.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_FIRE_MOUNTAIN_LOOKOUT_PLATFORM_DESTROY_THE_CANNONS, 0x47, KEY_FP_ITEM, "sea", "Room20", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x4F, 0x73, 0x00, 0x00}, 0x47, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room20.arc/ScalableObject001
    {KeyKind::Dzx, PCR_LOC_FIRE_MOUNTAIN_BIG_OCTO, 0x06, KEY_FP_ITEM, "sea", "Room20", "SCOB", 1, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x77, 0x53, 0x6C, 0x76, 0x67, 0x00, 0x00}, 0x06, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room40.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_ICE_RING_ISLE_FROZEN_CHEST, 0xD8, KEY_FP_ITEM, "sea", "Room40", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x00, 0x00}, 0xD8, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // MiniHyo/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_ICE_RING_ISLE_CAVE_CHEST, 0x29, KEY_FP_ITEM, "MiniHyo", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x33, 0x00}, 0x29, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // ITest62/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_ICE_RING_ISLE_INNER_CAVE_CHEST, 0x06, KEY_FP_ITEM, "ITest62", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x47, 0x63, 0x00, 0x00}, 0x06, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room45.arc/Actor013
    {KeyKind::Dzx, PCR_LOC_HEADSTONE_ISLAND_TOP_OF_THE_ISLAND, 0x07, KEY_FP_ITEM, "sea", "Room45", "ACTR", 19, DzxField::ItemItem, 0x08, 4, 0x000000FFu, {0x69, 0x74, 0x65, 0x6D, 0x00, 0x00, 0x00, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // Abship/Room3.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_HEADSTONE_ISLAND_SUBMARINE, 0xDD, KEY_FP_ITEM, "Abship", "Room3", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x35, 0x00}, 0xDD, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // Abship/Room5.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_HEADSTONE_ISLAND_SUBMARINE, 0xDD, KEY_FP_ITEM, "Abship", "Room5", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x35, 0x00}, 0xDD, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // M_Dai/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_EARTH_TEMPLE_TRANSPARENT_CHEST_IN_WARP_POT_ROOM, 0x4C, KEY_FP_ITEM, "M_Dai", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x42, 0x4D, 0x73, 0x00, 0x00}, 0x4C, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // M_Dai/Room2.arc/Actor010
    {KeyKind::Dzx, PCR_LOC_EARTH_TEMPLE_BEHIND_CURTAIN_IN_WARP_POT_ROOM, 0x1F, KEY_FP_ITEM, "M_Dai", "Room2", "ACTR", 16, DzxField::ItemItem, 0x08, 4, 0x000000FFu, {0x69, 0x74, 0x65, 0x6D, 0x00, 0x00, 0x00, 0x00}, 0x1F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // M_Dai/Stage.arc/Chest001
    {KeyKind::Dzx, PCR_LOC_EARTH_TEMPLE_TRANSPARENT_CHEST_IN_FIRST_CRYPT, 0x15, KEY_FP_ITEM, "M_Dai", "Stage", "TRES", 1, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x4D, 0x00}, 0x15, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // M_Dai/Stage.arc/Chest00A
    {KeyKind::Dzx, PCR_LOC_EARTH_TEMPLE_CHEST_BEHIND_DESTRUCTIBLE_WALLS, 0x1F, KEY_FP_ITEM, "M_Dai", "Stage", "TRES", 10, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x32, 0x00}, 0x1F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // M_Dai/Stage.arc/Chest002
    {KeyKind::Dzx, PCR_LOC_EARTH_TEMPLE_CHEST_IN_THREE_BLOCKS_ROOM, 0x4D, KEY_FP_ITEM, "M_Dai", "Stage", "TRES", 2, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x35, 0x00}, 0x4D, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // M_Dai/Stage.arc/Chest003
    {KeyKind::Dzx, PCR_LOC_EARTH_TEMPLE_CHEST_BEHIND_STATUES, 0x1F, KEY_FP_ITEM, "M_Dai", "Stage", "TRES", 3, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x32, 0x00}, 0x1F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // M_Dai/Room8.arc/Actor008
    {KeyKind::Dzx, PCR_LOC_EARTH_TEMPLE_CASKET_IN_SECOND_CRYPT, 0x15, KEY_FP_ITEM, "M_Dai", "Room8", "ACTR", 8, DzxField::ItemItem, 0x08, 4, 0x000000FFu, {0x69, 0x74, 0x65, 0x6D, 0x00, 0x00, 0x00, 0x00}, 0x15, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // M_DaiMB/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_EARTH_TEMPLE_STALFOS_MINIBOSS_ROOM, 0x3C, KEY_FP_ITEM, "M_DaiMB", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x33, 0x00}, 0x3C, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // M_Dai/Stage.arc/Chest00D
    {KeyKind::Dzx, PCR_LOC_EARTH_TEMPLE_TINGLE_STATUE_CHEST, 0xA6, KEY_FP_ITEM, "M_Dai", "Stage", "TRES", 13, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x36, 0x00}, 0xA6, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // M_Dai/Stage.arc/Chest004
    {KeyKind::Dzx, PCR_LOC_EARTH_TEMPLE_END_OF_FOGGY_ROOM_WITH_FLOORMASTERS, 0x15, KEY_FP_ITEM, "M_Dai", "Stage", "TRES", 4, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x32, 0x00}, 0x15, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // M_Dai/Stage.arc/Chest009
    {KeyKind::Dzx, PCR_LOC_EARTH_TEMPLE_KILL_ALL_FLOORMASTERS_IN_FOGGY_ROOM, 0xF3, KEY_FP_ITEM, "M_Dai", "Stage", "TRES", 9, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x37, 0x00}, 0xF3, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // M_Dai/Room13.arc/Actor01A
    {KeyKind::Dzx, PCR_LOC_EARTH_TEMPLE_BEHIND_CURTAIN_NEXT_TO_HAMMER_BUTTON, 0x1F, KEY_FP_ITEM, "M_Dai", "Room13", "ACTR", 26, DzxField::ItemItem, 0x08, 4, 0x000000FFu, {0x69, 0x74, 0x65, 0x6D, 0x00, 0x00, 0x00, 0x00}, 0x1F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // M_Dai/Stage.arc/Chest005
    {KeyKind::Dzx, PCR_LOC_EARTH_TEMPLE_CHEST_IN_THIRD_CRYPT, 0x04, KEY_FP_ITEM, "M_Dai", "Stage", "TRES", 5, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x32, 0x00}, 0x04, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // M_Dai/Stage.arc/Chest007
    {KeyKind::Dzx, PCR_LOC_EARTH_TEMPLE_MANY_MIRRORS_ROOM_RIGHT_CHEST, 0x05, KEY_FP_ITEM, "M_Dai", "Stage", "TRES", 7, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x4D, 0x00}, 0x05, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // M_Dai/Stage.arc/Chest008
    {KeyKind::Dzx, PCR_LOC_EARTH_TEMPLE_MANY_MIRRORS_ROOM_LEFT_CHEST, 0x1F, KEY_FP_ITEM, "M_Dai", "Stage", "TRES", 8, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x4D, 0x00}, 0x1F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // M_Dai/Stage.arc/Chest00B
    {KeyKind::Dzx, PCR_LOC_EARTH_TEMPLE_STALFOS_CRYPT_ROOM, 0xE9, KEY_FP_ITEM, "M_Dai", "Stage", "TRES", 11, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x37, 0x00}, 0xE9, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // M_Dai/Stage.arc/Chest006
    {KeyKind::Dzx, PCR_LOC_EARTH_TEMPLE_BIG_KEY_CHEST, 0x4E, KEY_FP_ITEM, "M_Dai", "Stage", "TRES", 6, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x34, 0x00}, 0x4E, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // rels/d_a_bpw.rel@877E
    {KeyKind::Code, PCR_LOC_EARTH_TEMPLE_JALHALLA_HEART_CONTAINER, 0x08, 0, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x00, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_bpw.rel", 0x0000877Eu, PCR_ADDED_NONE},
    // M_DaiB/Room0.arc/Actor001
    {KeyKind::Dzx, PCR_LOC_EARTH_TEMPLE_JALHALLA_HEART_CONTAINER, 0x08, 0, "M_DaiB", "Room0", "ACTR", 1, DzxField::BossItem, 0x08, 4, 0x0000FF00u, {0x42, 0x69, 0x74, 0x65, 0x6D, 0x00, 0x00, 0x00}, 0x00, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // kaze/Stage.arc/Chest005
    {KeyKind::Dzx, PCR_LOC_WIND_TEMPLE_CHEST_BETWEEN_TWO_DIRT_PATCHES, 0x06, KEY_FP_ITEM, "kaze", "Stage", "TRES", 5, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x53, 0x77, 0x00, 0x00}, 0x06, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // kaze/Room1.arc/Actor010
    {KeyKind::Dzx, PCR_LOC_WIND_TEMPLE_BEHIND_STONE_HEAD_IN_HIDDEN_UPPER_ROOM, 0x1F, 0, "kaze", "Room1", "ACTR", 16, DzxField::HomenItem, 0x18, 2, 0x000000FFu, {0x48, 0x6F, 0x6D, 0x65, 0x6E, 0x32, 0x00, 0x00}, 0x00, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // kaze/Stage.arc/Chest00C
    {KeyKind::Dzx, PCR_LOC_WIND_TEMPLE_TINGLE_STATUE_CHEST, 0xA7, KEY_FP_ITEM, "kaze", "Stage", "TRES", 12, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x36, 0x00}, 0xA7, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // kaze/Stage.arc/Chest006
    {KeyKind::Dzx, PCR_LOC_WIND_TEMPLE_CHEST_BEHIND_STONE_HEAD, 0x1F, KEY_FP_ITEM, "kaze", "Stage", "TRES", 6, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x32, 0x00}, 0x1F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // kaze/Stage.arc/Chest004
    {KeyKind::Dzx, PCR_LOC_WIND_TEMPLE_CHEST_IN_LEFT_ALCOVE, 0x1F, KEY_FP_ITEM, "kaze", "Stage", "TRES", 4, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x32, 0x00}, 0x1F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // kaze/Stage.arc/Chest002
    {KeyKind::Dzx, PCR_LOC_WIND_TEMPLE_BIG_KEY_CHEST, 0x4E, KEY_FP_ITEM, "kaze", "Stage", "TRES", 2, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x34, 0x00}, 0x4E, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // kaze/Stage.arc/Chest008
    {KeyKind::Dzx, PCR_LOC_WIND_TEMPLE_CHEST_IN_MANY_CYCLONES_ROOM, 0x4C, KEY_FP_ITEM, "kaze", "Stage", "TRES", 8, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x00, 0x00}, 0x4C, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // kaze/Room7.arc/Actor02E
    {KeyKind::Dzx, PCR_LOC_WIND_TEMPLE_BEHIND_STONE_HEAD_IN_MANY_CYCLONES_ROOM, 0x1F, 0, "kaze", "Room7", "ACTR", 46, DzxField::HomenItem, 0x18, 2, 0x000000FFu, {0x48, 0x6F, 0x6D, 0x65, 0x6E, 0x32, 0x00, 0x00}, 0x00, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // kaze/Stage.arc/Chest00A
    {KeyKind::Dzx, PCR_LOC_WIND_TEMPLE_CHEST_IN_MIDDLE_OF_HUB_ROOM, 0x4D, KEY_FP_ITEM, "kaze", "Stage", "TRES", 10, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x00, 0x00}, 0x4D, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // kaze/Stage.arc/Chest003
    {KeyKind::Dzx, PCR_LOC_WIND_TEMPLE_SPIKE_WALL_ROOM_FIRST_CHEST, 0x15, KEY_FP_ITEM, "kaze", "Stage", "TRES", 3, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x32, 0x00}, 0x15, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // kaze/Stage.arc/Chest007
    {KeyKind::Dzx, PCR_LOC_WIND_TEMPLE_SPIKE_WALL_ROOM_DESTROY_ALL_CRACKED_FLOORS, 0xE8, KEY_FP_ITEM, "kaze", "Stage", "TRES", 7, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x35, 0x00}, 0xE8, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // kazeMB/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_WIND_TEMPLE_WIZZROBE_MINIBOSS_ROOM, 0x2F, KEY_FP_ITEM, "kazeMB", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x38, 0x00}, 0x2F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // kaze/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_WIND_TEMPLE_CHEST_AT_TOP_OF_HUB_ROOM, 0x1F, KEY_FP_ITEM, "kaze", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x32, 0x00}, 0x1F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // kaze/Stage.arc/Chest001
    {KeyKind::Dzx, PCR_LOC_WIND_TEMPLE_CHEST_BEHIND_SEVEN_ARMOS, 0x15, KEY_FP_ITEM, "kaze", "Stage", "TRES", 1, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x32, 0x00}, 0x15, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // kaze/Stage.arc/Chest009
    {KeyKind::Dzx, PCR_LOC_WIND_TEMPLE_KILL_ALL_ENEMIES_IN_TALL_BASEMENT_ROOM, 0xF2, KEY_FP_ITEM, "kaze", "Stage", "TRES", 9, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x37, 0x00}, 0xF2, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // rels/d_a_bwd.rel@6D83
    {KeyKind::Code, PCR_LOC_WIND_TEMPLE_MOLGERA_HEART_CONTAINER, 0x08, 0, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x00, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_bwd.rel", 0x00006D83u, PCR_ADDED_NONE},
    // kazeB/Room0.arc/Actor001
    {KeyKind::Dzx, PCR_LOC_WIND_TEMPLE_MOLGERA_HEART_CONTAINER, 0x08, 0, "kazeB", "Room0", "ACTR", 1, DzxField::BossItem, 0x08, 4, 0x0000FF00u, {0x42, 0x69, 0x74, 0x65, 0x6D, 0x00, 0x00, 0x00}, 0x00, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // GanonJ/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_GANONS_TOWER_MAZE_CHEST, 0x36, KEY_FP_ITEM, "GanonJ", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x38, 0x00}, 0x36, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // rels/d_a_obj_toripost.rel@25DC
    {KeyKind::Code, PCR_LOC_MAILBOX_LETTER_FROM_HOSKITS_GIRLFRIEND, 0x3F, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x3F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_obj_toripost.rel", 0x000025DCu, PCR_ADDED_NONE},
    // rels/d_a_obj_toripost.rel@2564
    {KeyKind::Code, PCR_LOC_MAILBOX_LETTER_FROM_BAITOS_MOTHER, 0x07, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_obj_toripost.rel", 0x00002564u, PCR_ADDED_NONE},
    // rels/d_a_obj_toripost.rel@25C4
    {KeyKind::Code, PCR_LOC_MAILBOX_LETTER_FROM_BAITOS_MOTHER, 0x07, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_obj_toripost.rel", 0x000025C4u, PCR_ADDED_NONE},
    // rels/d_a_obj_toripost.rel@25E8
    {KeyKind::Code, PCR_LOC_MAILBOX_LETTER_FROM_BAITO, 0x04, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x04, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_obj_toripost.rel", 0x000025E8u, PCR_ADDED_NONE},
    // rels/d_a_obj_toripost.rel@2570
    {KeyKind::Code, PCR_LOC_MAILBOX_LETTER_FROM_KOMALIS_FATHER, 0x07, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_obj_toripost.rel", 0x00002570u, PCR_ADDED_NONE},
    // rels/d_a_obj_toripost.rel@257C
    {KeyKind::Code, PCR_LOC_MAILBOX_LETTER_ADVERTISING_BOMBS_IN_BEEDLES_SHOP, 0xC3, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0xC3, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_obj_toripost.rel", 0x0000257Cu, PCR_ADDED_NONE},
    // rels/d_a_obj_toripost.rel@25A0
    {KeyKind::Code, PCR_LOC_MAILBOX_LETTER_ADVERTISING_ROCK_SPIRE_SHOP_SHIP, 0x01, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x01, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_obj_toripost.rel", 0x000025A0u, PCR_ADDED_NONE},
    // rels/d_a_obj_toripost.rel@25D0
    {KeyKind::Code, PCR_LOC_MAILBOX_BEEDLES_SILVER_MEMBERSHIP_REWARD, 0x9D, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x9D, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_obj_toripost.rel", 0x000025D0u, PCR_ADDED_NONE},
    // rels/d_a_obj_toripost.rel@25F4
    {KeyKind::Code, PCR_LOC_MAILBOX_BEEDLES_GOLD_MEMBERSHIP_REWARD, 0x9E, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x9E, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_obj_toripost.rel", 0x000025F4u, PCR_ADDED_NONE},
    // rels/d_a_obj_toripost.rel@2588
    {KeyKind::Code, PCR_LOC_MAILBOX_LETTER_FROM_ORCA, 0x04, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x04, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_obj_toripost.rel", 0x00002588u, PCR_ADDED_NONE},
    // rels/d_a_obj_toripost.rel@2594
    {KeyKind::Code, PCR_LOC_MAILBOX_LETTER_FROM_GRANDMA, 0x04, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x04, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_obj_toripost.rel", 0x00002594u, PCR_ADDED_NONE},
    // rels/d_a_obj_toripost.rel@25B8
    {KeyKind::Code, PCR_LOC_MAILBOX_LETTER_FROM_ARYLL, 0x04, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x04, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_obj_toripost.rel", 0x000025B8u, PCR_ADDED_NONE},
    // rels/d_a_obj_toripost.rel@25AC
    {KeyKind::Code, PCR_LOC_MAILBOX_LETTER_FROM_TINGLE, 0xCB, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0xCB, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_obj_toripost.rel", 0x000025ACu, PCR_ADDED_NONE},
    // main.dol@80375E1F
    {KeyKind::Code, PCR_LOC_THE_GREAT_SEA_BEEDLES_SHOP_SHIP_20_RUPEE_ITEM, 0x2C, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x2C, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::MainDol, "main.dol", 0x80375E1Fu, PCR_ADDED_NONE},
    // rels/d_a_npc_bs1.rel@1CDB
    {KeyKind::Code, PCR_LOC_THE_GREAT_SEA_BEEDLES_SHOP_SHIP_20_RUPEE_ITEM, 0x2C, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x2C, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_bs1.rel", 0x00001CDBu, PCR_ADDED_NONE},
    // rels/d_a_npc_bs1.rel@2DBB
    {KeyKind::Code, PCR_LOC_THE_GREAT_SEA_BEEDLES_SHOP_SHIP_20_RUPEE_ITEM, 0x2C, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x2C, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_bs1.rel", 0x00002DBBu, PCR_ADDED_NONE},
    // rels/d_a_npc_sv.rel@3327
    {KeyKind::Code, PCR_LOC_THE_GREAT_SEA_SALVAGE_CORP_GIFT, 0xE5, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0xE5, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_sv.rel", 0x00003327u, PCR_ADDED_NONE},
    // sea/Stage.arc/Event0E0:TACT1_RT/Actor001/Action003
    {KeyKind::Event, PCR_LOC_THE_GREAT_SEA_CYCLOS, 0x6E, KEY_FP_ITEM, "sea", "Stage", nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0, "TACT1_RT", 224, 1, 3, 3106, 1677, "059get_dance", 1, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // rels/d_a_npc_roten.rel@533B
    {KeyKind::Code, PCR_LOC_THE_GREAT_SEA_GORON_TRADING_REWARD, 0x07, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_roten.rel", 0x0000533Bu, PCR_ADDED_NONE},
    // rels/d_a_obj_ftree.rel@0237
    {KeyKind::Code, PCR_LOC_THE_GREAT_SEA_WITHERED_TREES, 0x07, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_obj_ftree.rel", 0x00000237u, PCR_ADDED_NONE},
    // rels/d_a_obj_ftree.rel@03F3
    {KeyKind::Code, PCR_LOC_THE_GREAT_SEA_WITHERED_TREES, 0x07, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_obj_ftree.rel", 0x000003F3u, PCR_ADDED_NONE},
    // PShip/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_THE_GREAT_SEA_GHOST_SHIP, 0xFB, KEY_FP_ITEM, "PShip", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x33, 0x00}, 0xFB, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // PShip/Stage.arc/Chest001
    {KeyKind::Dzx, PCR_LOC_THE_GREAT_SEA_GHOST_SHIP, 0xFB, KEY_FP_ITEM, "PShip", "Stage", "TRES", 1, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x33, 0x00}, 0xFB, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // PShip/Stage.arc/Chest002
    {KeyKind::Dzx, PCR_LOC_THE_GREAT_SEA_GHOST_SHIP, 0xFB, KEY_FP_ITEM, "PShip", "Stage", "TRES", 2, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x33, 0x00}, 0xFB, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // PShip2/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_THE_GREAT_SEA_GHOST_SHIP, 0xFB, KEY_FP_ITEM, "PShip2", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x38, 0x00}, 0xFB, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // PShip3/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_THE_GREAT_SEA_GHOST_SHIP, 0xFB, KEY_FP_ITEM, "PShip3", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x38, 0x00}, 0xFB, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room33.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_PRIVATE_OASIS_CHEST_AT_TOP_OF_WATERFALL, 0xCD, KEY_FP_ITEM, "sea", "Room33", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x33, 0x00}, 0xCD, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // TF_04/Stage.arc/Chest001
    {KeyKind::Dzx, PCR_LOC_PRIVATE_OASIS_CABANA_LABYRINTH_LOWER_FLOOR_CHEST, 0x06, KEY_FP_ITEM, "TF_04", "Stage", "TRES", 1, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x32, 0x00}, 0x06, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // TF_04/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_PRIVATE_OASIS_CABANA_LABYRINTH_UPPER_FLOOR_CHEST, 0xFD, KEY_FP_ITEM, "TF_04", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x43, 0x54, 0x66, 0x00, 0x00}, 0xFD, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room33.arc/ScalableObject015
    {KeyKind::Dzx, PCR_LOC_PRIVATE_OASIS_BIG_OCTO, 0x06, KEY_FP_ITEM, "sea", "Room33", "SCOB", 21, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x77, 0x53, 0x6C, 0x76, 0x67, 0x00, 0x00}, 0x06, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // rels/d_a_npc_kg2.rel@20FF
    {KeyKind::Code, PCR_LOC_SPECTACLE_ISLAND_BARREL_SHOOTING_FIRST_PRIZE, 0x07, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_kg2.rel", 0x000020FFu, PCR_ADDED_NONE},
    // rels/d_a_npc_kg2.rel@2107
    {KeyKind::Code, PCR_LOC_SPECTACLE_ISLAND_BARREL_SHOOTING_SECOND_PRIZE, 0xD7, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0xD7, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_kg2.rel", 0x00002107u, PCR_ADDED_NONE},
    // sea/Room29.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_NEEDLE_ROCK_ISLE_CHEST, 0x07, KEY_FP_ITEM, "sea", "Room29", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x33, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // SubD42/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_NEEDLE_ROCK_ISLE_CAVE, 0x06, KEY_FP_ITEM, "SubD42", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x53, 0x77, 0x00, 0x00}, 0x06, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room29.arc/ScalableObject00D
    {KeyKind::Dzx, PCR_LOC_NEEDLE_ROCK_ISLE_GOLDEN_GUNBOAT, 0xFA, KEY_FP_ITEM, "sea", "Room29", "SCOB", 13, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x77, 0x53, 0x6C, 0x76, 0x67, 0x00, 0x00}, 0xFA, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room47.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_ANGULAR_ISLES_PEAK, 0x07, KEY_FP_ITEM, "sea", "Room47", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x33, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // SubD43/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_ANGULAR_ISLES_CAVE, 0x0F, KEY_FP_ITEM, "SubD43", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x4D, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room48.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_BOATING_COURSE_RAFT, 0x46, KEY_FP_ITEM, "sea", "Room48", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x49, 0x6B, 0x00, 0x00}, 0x46, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // SubD71/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_BOATING_COURSE_CAVE, 0xC2, KEY_FP_ITEM, "SubD71", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x35, 0x00}, 0xC2, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // TF_01/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_STONE_WATCHER_ISLAND_CAVE, 0xF8, KEY_FP_ITEM, "TF_01", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x43, 0x54, 0x66, 0x00, 0x00}, 0xF8, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room31.arc/Chest001
    {KeyKind::Dzx, PCR_LOC_STONE_WATCHER_ISLAND_LOOKOUT_PLATFORM_CHEST, 0x04, KEY_FP_ITEM, "sea", "Room31", "TRES", 1, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x4F, 0x63, 0x00, 0x00}, 0x04, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room31.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_STONE_WATCHER_ISLAND_LOOKOUT_PLATFORM_DESTROY_THE_CANNONS, 0x07, KEY_FP_ITEM, "sea", "Room31", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x36, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // ShipD/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_ISLET_OF_STEEL_INTERIOR, 0xFE, KEY_FP_ITEM, "ShipD", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x43, 0x54, 0x66, 0x00, 0x00}, 0xFE, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room30.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_ISLET_OF_STEEL_LOOKOUT_PLATFORM_DEFEAT_THE_ENEMIES, 0x45, KEY_FP_ITEM, "sea", "Room30", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x4F, 0x73, 0x00, 0x00}, 0x45, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // TF_02/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_OVERLOOK_ISLAND_CAVE, 0xF7, KEY_FP_ITEM, "TF_02", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x43, 0x54, 0x66, 0x00, 0x00}, 0xF7, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // TF_03/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_BIRDS_PEAK_ROCK_CAVE, 0xFC, KEY_FP_ITEM, "TF_03", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x43, 0x54, 0x66, 0x00, 0x00}, 0xFC, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // TyuTyu/Stage.arc/Chest003
    {KeyKind::Dzx, PCR_LOC_PAWPRINT_ISLE_CHUCHU_CAVE_CHEST, 0x07, KEY_FP_ITEM, "TyuTyu", "Stage", "TRES", 3, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x33, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // TyuTyu/Stage.arc/Chest001
    {KeyKind::Dzx, PCR_LOC_PAWPRINT_ISLE_CHUCHU_CAVE_BEHIND_LEFT_BOULDER, 0x05, KEY_FP_ITEM, "TyuTyu", "Stage", "TRES", 1, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x32, 0x00}, 0x05, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // TyuTyu/Stage.arc/Chest002
    {KeyKind::Dzx, PCR_LOC_PAWPRINT_ISLE_CHUCHU_CAVE_BEHIND_RIGHT_BOULDER, 0x1F, KEY_FP_ITEM, "TyuTyu", "Stage", "TRES", 2, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x32, 0x00}, 0x1F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // TyuTyu/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_PAWPRINT_ISLE_CHUCHU_CAVE_SCALE_THE_WALL, 0x04, KEY_FP_ITEM, "TyuTyu", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x32, 0x00}, 0x04, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // Cave07/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_PAWPRINT_ISLE_WIZZROBE_CAVE, 0x0F, KEY_FP_ITEM, "Cave07", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x47, 0x63, 0x00, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room12.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_PAWPRINT_ISLE_LOOKOUT_PLATFORM_DEFEAT_THE_ENEMIES, 0x04, KEY_FP_ITEM, "sea", "Room12", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x4F, 0x73, 0x00, 0x00}, 0x04, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // rels/d_a_bigelf.rel@221F
    {KeyKind::Code, PCR_LOC_THORNED_FAIRY_ISLAND_GREAT_FAIRY, 0xB0, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0xB0, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_bigelf.rel", 0x0000221Fu, PCR_ADDED_NONE},
    // sea/Room28.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_THORNED_FAIRY_ISLAND_NORTHEASTERN_LOOKOUT_PLATFORM_DESTROY_THE_CANNONS, 0x47, KEY_FP_ITEM, "sea", "Room28", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x4F, 0x73, 0x00, 0x00}, 0x47, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room28.arc/Chest001
    {KeyKind::Dzx, PCR_LOC_THORNED_FAIRY_ISLAND_SOUTHWESTERN_LOOKOUT_PLATFORM_DEFEAT_THE_ENEMIES, 0x46, KEY_FP_ITEM, "sea", "Room28", "TRES", 1, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x4F, 0x73, 0x00, 0x00}, 0x46, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // rels/d_a_bigelf.rel@21E7
    {KeyKind::Code, PCR_LOC_EASTERN_FAIRY_ISLAND_GREAT_FAIRY, 0xAD, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0xAD, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_bigelf.rel", 0x000021E7u, PCR_ADDED_NONE},
    // sea/Room19.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_EASTERN_FAIRY_ISLAND_LOOKOUT_PLATFORM_DEFEAT_THE_CANNONS_AND_ENEMIES, 0x46, KEY_FP_ITEM, "sea", "Room19", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x4F, 0x73, 0x00, 0x00}, 0x46, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // rels/d_a_bigelf.rel@2213
    {KeyKind::Code, PCR_LOC_WESTERN_FAIRY_ISLAND_GREAT_FAIRY, 0xAF, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0xAF, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_bigelf.rel", 0x00002213u, PCR_ADDED_NONE},
    // sea/Room15.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_WESTERN_FAIRY_ISLAND_LOOKOUT_PLATFORM, 0x45, KEY_FP_ITEM, "sea", "Room15", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x4F, 0x63, 0x00, 0x00}, 0x45, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // rels/d_a_bigelf.rel@21F3
    {KeyKind::Code, PCR_LOC_SOUTHERN_FAIRY_ISLAND_GREAT_FAIRY, 0xAE, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0xAE, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_bigelf.rel", 0x000021F3u, PCR_ADDED_NONE},
    // sea/Room39.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_SOUTHERN_FAIRY_ISLAND_LOOKOUT_PLATFORM_DESTROY_THE_NORTHWEST_CANNONS, 0xDF, KEY_FP_ITEM, "sea", "Room39", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x35, 0x00}, 0xDF, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room39.arc/Chest001
    {KeyKind::Dzx, PCR_LOC_SOUTHERN_FAIRY_ISLAND_LOOKOUT_PLATFORM_DESTROY_THE_SOUTHEAST_CANNONS, 0x45, KEY_FP_ITEM, "sea", "Room39", "TRES", 1, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x4F, 0x73, 0x00, 0x00}, 0x45, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // rels/d_a_bigelf.rel@21BB
    {KeyKind::Code, PCR_LOC_NORTHERN_FAIRY_ISLAND_GREAT_FAIRY, 0xAB, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0xAB, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_bigelf.rel", 0x000021BBu, PCR_ADDED_NONE},
    // Abship/Room4.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_NORTHERN_FAIRY_ISLAND_SUBMARINE, 0xD9, KEY_FP_ITEM, "Abship", "Room4", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x35, 0x00}, 0xD9, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // rels/d_a_npc_tc.rel@6143
    {KeyKind::Code, PCR_LOC_TINGLE_ISLAND_ANKLE_REWARD_FOR_ALL_TINGLE_STATUES, 0xB8, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0xB8, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_tc.rel", 0x00006143u, PCR_ADDED_NONE},
    // sea/Room17.arc/ScalableObject001
    {KeyKind::Dzx, PCR_LOC_TINGLE_ISLAND_BIG_OCTO, 0x07, KEY_FP_ITEM, "sea", "Room17", "SCOB", 1, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x77, 0x53, 0x6C, 0x76, 0x67, 0x00, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // WarpD/Stage.arc/Chest001
    {KeyKind::Dzx, PCR_LOC_DIAMOND_STEPPE_ISLAND_WARP_MAZE_CAVE_FIRST_CHEST, 0x1F, KEY_FP_ITEM, "WarpD", "Stage", "TRES", 1, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x32, 0x00}, 0x1F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // WarpD/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_DIAMOND_STEPPE_ISLAND_WARP_MAZE_CAVE_SECOND_CHEST, 0xDB, KEY_FP_ITEM, "WarpD", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x00, 0x00}, 0xDB, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room36.arc/ScalableObject001
    {KeyKind::Dzx, PCR_LOC_DIAMOND_STEPPE_ISLAND_BIG_OCTO, 0x06, KEY_FP_ITEM, "sea", "Room36", "SCOB", 1, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x77, 0x53, 0x6C, 0x76, 0x67, 0x00, 0x00}, 0x06, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // Cave01/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_BOMB_ISLAND_CAVE, 0x07, KEY_FP_ITEM, "Cave01", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x33, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room34.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_BOMB_ISLAND_LOOKOUT_PLATFORM_DEFEAT_THE_ENEMIES, 0x04, KEY_FP_ITEM, "sea", "Room34", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x4F, 0x73, 0x00, 0x00}, 0x04, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // Abship/Room2.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_BOMB_ISLAND_SUBMARINE, 0x50, KEY_FP_ITEM, "Abship", "Room2", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x53, 0x77, 0x00, 0x00}, 0x50, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // Cave04/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_ROCK_SPIRE_ISLE_CAVE, 0xD5, KEY_FP_ITEM, "Cave04", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x37, 0x00}, 0xD5, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // main.dol@80375ECF
    {KeyKind::Code, PCR_LOC_ROCK_SPIRE_ISLE_BEEDLES_SPECIAL_SHOP_SHIP_500_RUPEE_ITEM, 0x50, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x50, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::MainDol, "main.dol", 0x80375ECFu, PCR_ADDED_NONE},
    // rels/d_a_npc_bs1.rel@1E9B
    {KeyKind::Code, PCR_LOC_ROCK_SPIRE_ISLE_BEEDLES_SPECIAL_SHOP_SHIP_500_RUPEE_ITEM, 0x50, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x50, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_bs1.rel", 0x00001E9Bu, PCR_ADDED_NONE},
    // rels/d_a_npc_bs1.rel@1ED3
    {KeyKind::Code, PCR_LOC_ROCK_SPIRE_ISLE_BEEDLES_SPECIAL_SHOP_SHIP_500_RUPEE_ITEM, 0x50, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x50, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_bs1.rel", 0x00001ED3u, PCR_ADDED_NONE},
    // main.dol@80375EDF
    {KeyKind::Code, PCR_LOC_ROCK_SPIRE_ISLE_BEEDLES_SPECIAL_SHOP_SHIP_950_RUPEE_ITEM, 0x07, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::MainDol, "main.dol", 0x80375EDFu, PCR_ADDED_NONE},
    // rels/d_a_npc_bs1.rel@1EA3
    {KeyKind::Code, PCR_LOC_ROCK_SPIRE_ISLE_BEEDLES_SPECIAL_SHOP_SHIP_950_RUPEE_ITEM, 0x07, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_bs1.rel", 0x00001EA3u, PCR_ADDED_NONE},
    // rels/d_a_npc_bs1.rel@1EDF
    {KeyKind::Code, PCR_LOC_ROCK_SPIRE_ISLE_BEEDLES_SPECIAL_SHOP_SHIP_950_RUPEE_ITEM, 0x07, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_bs1.rel", 0x00001EDFu, PCR_ADDED_NONE},
    // main.dol@80375EEF
    {KeyKind::Code, PCR_LOC_ROCK_SPIRE_ISLE_BEEDLES_SPECIAL_SHOP_SHIP_900_RUPEE_ITEM, 0xE1, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0xE1, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::MainDol, "main.dol", 0x80375EEFu, PCR_ADDED_NONE},
    // rels/d_a_npc_bs1.rel@1EAB
    {KeyKind::Code, PCR_LOC_ROCK_SPIRE_ISLE_BEEDLES_SPECIAL_SHOP_SHIP_900_RUPEE_ITEM, 0xE1, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0xE1, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_bs1.rel", 0x00001EABu, PCR_ADDED_NONE},
    // rels/d_a_npc_bs1.rel@1EEB
    {KeyKind::Code, PCR_LOC_ROCK_SPIRE_ISLE_BEEDLES_SPECIAL_SHOP_SHIP_900_RUPEE_ITEM, 0xE1, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0xE1, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_bs1.rel", 0x00001EEBu, PCR_ADDED_NONE},
    // sea/Room16.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_ROCK_SPIRE_ISLE_WESTERN_LOOKOUT_PLATFORM_DESTROY_THE_CANNONS, 0x06, KEY_FP_ITEM, "sea", "Room16", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x4F, 0x73, 0x00, 0x00}, 0x06, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room16.arc/Chest001
    {KeyKind::Dzx, PCR_LOC_ROCK_SPIRE_ISLE_EASTERN_LOOKOUT_PLATFORM_DESTROY_THE_CANNONS, 0x47, KEY_FP_ITEM, "sea", "Room16", "TRES", 1, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x4F, 0x73, 0x00, 0x00}, 0x47, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room16.arc/Chest002
    {KeyKind::Dzx, PCR_LOC_ROCK_SPIRE_ISLE_CENTER_LOOKOUT_PLATFORM, 0x04, KEY_FP_ITEM, "sea", "Room16", "TRES", 2, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x4F, 0x63, 0x00, 0x00}, 0x04, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room16.arc/ScalableObject000
    {KeyKind::Dzx, PCR_LOC_ROCK_SPIRE_ISLE_SOUTHEAST_GUNBOAT, 0x07, KEY_FP_ITEM, "sea", "Room16", "SCOB", 0, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x77, 0x53, 0x6C, 0x76, 0x67, 0x00, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // ITest63/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_SHARK_ISLAND_CAVE, 0x0F, KEY_FP_ITEM, "ITest63", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x53, 0x77, 0x00, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // Cave03/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_CLIFF_PLATEAU_ISLES_CAVE, 0x1F, KEY_FP_ITEM, "Cave03", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x32, 0x00}, 0x1F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room42.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_CLIFF_PLATEAU_ISLES_HIGHEST_ISLE, 0xD6, KEY_FP_ITEM, "sea", "Room42", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x00, 0x00}, 0xD6, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room42.arc/Chest001
    {KeyKind::Dzx, PCR_LOC_CLIFF_PLATEAU_ISLES_LOOKOUT_PLATFORM, 0x46, KEY_FP_ITEM, "sea", "Room42", "TRES", 1, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x4F, 0x63, 0x00, 0x00}, 0x46, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room5.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_CRESCENT_MOON_ISLAND_CHEST, 0xDE, KEY_FP_ITEM, "sea", "Room5", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x00, 0x00}, 0xDE, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // Abship/Room7.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_CRESCENT_MOON_ISLAND_SUBMARINE, 0xDA, KEY_FP_ITEM, "Abship", "Room7", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x00, 0x00}, 0xDA, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room43.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_HORSESHOE_ISLAND_PLAY_GOLF, 0xE2, KEY_FP_ITEM, "sea", "Room43", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x35, 0x00}, 0xE2, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // Cave05/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_HORSESHOE_ISLAND_CAVE, 0xD4, KEY_FP_ITEM, "Cave05", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x37, 0x00}, 0xD4, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room43.arc/Chest001
    {KeyKind::Dzx, PCR_LOC_HORSESHOE_ISLAND_NORTHWESTERN_LOOKOUT_PLATFORM, 0x45, KEY_FP_ITEM, "sea", "Room43", "TRES", 1, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x4F, 0x73, 0x00, 0x00}, 0x45, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room43.arc/Chest002
    {KeyKind::Dzx, PCR_LOC_HORSESHOE_ISLAND_SOUTHEASTERN_LOOKOUT_PLATFORM, 0x47, KEY_FP_ITEM, "sea", "Room43", "TRES", 2, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x4F, 0x73, 0x00, 0x00}, 0x47, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // rels/d_a_npc_bmcon1.rel@2B7B
    {KeyKind::Code, PCR_LOC_FLIGHT_CONTROL_PLATFORM_BIRD_MAN_CONTEST_FIRST_PRIZE, 0x07, KEY_FP_ITEM, nullptr, nullptr, nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::Rel, "rels/d_a_npc_bmcon1.rel", 0x00002B7Bu, PCR_ADDED_NONE},
    // Abship/Room6.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_FLIGHT_CONTROL_PLATFORM_SUBMARINE, 0xC4, KEY_FP_ITEM, "Abship", "Room6", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x35, 0x00}, 0xC4, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // Cave02/Stage.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_STAR_ISLAND_CAVE, 0x07, KEY_FP_ITEM, "Cave02", "Stage", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x38, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room2.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_STAR_ISLAND_LOOKOUT_PLATFORM, 0x47, KEY_FP_ITEM, "sea", "Room2", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x4F, 0x63, 0x00, 0x00}, 0x47, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room21.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_STAR_BELT_ARCHIPELAGO_LOOKOUT_PLATFORM, 0x04, KEY_FP_ITEM, "sea", "Room21", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x4F, 0x63, 0x00, 0x00}, 0x04, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room49.arc/Chest001
    {KeyKind::Dzx, PCR_LOC_FIVE_STAR_ISLES_LOOKOUT_PLATFORM_DESTROY_THE_CANNONS, 0x47, KEY_FP_ITEM, "sea", "Room49", "TRES", 1, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x4F, 0x73, 0x00, 0x00}, 0x47, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room49.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_FIVE_STAR_ISLES_RAFT, 0x04, KEY_FP_ITEM, "sea", "Room49", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x49, 0x6B, 0x00, 0x00}, 0x04, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // Abship/Room0.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_FIVE_STAR_ISLES_SUBMARINE, 0x07, KEY_FP_ITEM, "Abship", "Room0", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x36, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room6.arc/Chest001
    {KeyKind::Dzx, PCR_LOC_SEVEN_STAR_ISLES_CENTER_LOOKOUT_PLATFORM, 0x04, KEY_FP_ITEM, "sea", "Room6", "TRES", 1, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x4F, 0x73, 0x00, 0x00}, 0x04, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room6.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_SEVEN_STAR_ISLES_NORTHERN_LOOKOUT_PLATFORM, 0x47, KEY_FP_ITEM, "sea", "Room6", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x4F, 0x73, 0x00, 0x00}, 0x47, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room6.arc/Chest002
    {KeyKind::Dzx, PCR_LOC_SEVEN_STAR_ISLES_SOUTHERN_LOOKOUT_PLATFORM, 0xE3, KEY_FP_ITEM, "sea", "Room6", "TRES", 2, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x35, 0x00}, 0xE3, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room6.arc/ScalableObject000
    {KeyKind::Dzx, PCR_LOC_SEVEN_STAR_ISLES_BIG_OCTO, 0x07, KEY_FP_ITEM, "sea", "Room6", "SCOB", 0, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x77, 0x53, 0x6C, 0x76, 0x67, 0x00, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room24.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_CYCLOPS_REEF_DESTROY_THE_CANNONS_AND_GUNBOATS, 0xCE, KEY_FP_ITEM, "sea", "Room24", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x35, 0x00}, 0xCE, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room24.arc/Chest001
    {KeyKind::Dzx, PCR_LOC_CYCLOPS_REEF_LOOKOUT_PLATFORM_DEFEAT_THE_ENEMIES, 0x47, KEY_FP_ITEM, "sea", "Room24", "TRES", 1, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x4F, 0x73, 0x00, 0x00}, 0x47, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room46.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_TWO_EYE_REEF_DESTROY_THE_CANNONS_AND_GUNBOATS, 0xCF, KEY_FP_ITEM, "sea", "Room46", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x35, 0x00}, 0xCF, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room46.arc/Chest001
    {KeyKind::Dzx, PCR_LOC_TWO_EYE_REEF_LOOKOUT_PLATFORM, 0x05, KEY_FP_ITEM, "sea", "Room46", "TRES", 1, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x4F, 0x63, 0x00, 0x00}, 0x05, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Stage.arc/Event02A:BIGELF_ARRIVAL2/Actor001/Action002
    {KeyKind::Event, PCR_LOC_TWO_EYE_REEF_BIG_OCTO_GREAT_FAIRY, 0xB2, KEY_FP_ITEM, "sea", "Stage", nullptr, 0, DzxField::None, 0, 0, 0u, {}, 0, "BIGELF_ARRIVAL2", 42, 1, 2, 403, 184, "046pget", 178, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room22.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_THREE_EYE_REEF_DESTROY_THE_CANNONS_AND_GUNBOATS, 0xD0, KEY_FP_ITEM, "sea", "Room22", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x35, 0x00}, 0xD0, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room8.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_FOUR_EYE_REEF_DESTROY_THE_CANNONS_AND_GUNBOATS, 0xD1, KEY_FP_ITEM, "sea", "Room8", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x35, 0x00}, 0xD1, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room37.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_FIVE_EYE_REEF_DESTROY_THE_CANNONS, 0xD2, KEY_FP_ITEM, "sea", "Room37", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x35, 0x00}, 0xD2, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room37.arc/Chest001
    {KeyKind::Dzx, PCR_LOC_FIVE_EYE_REEF_LOOKOUT_PLATFORM, 0x47, KEY_FP_ITEM, "sea", "Room37", "TRES", 1, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x4F, 0x63, 0x00, 0x00}, 0x47, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room25.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_SIX_EYE_REEF_DESTROY_THE_CANNONS_AND_GUNBOATS, 0xD3, KEY_FP_ITEM, "sea", "Room25", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x35, 0x00}, 0xD3, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room25.arc/Chest001
    {KeyKind::Dzx, PCR_LOC_SIX_EYE_REEF_LOOKOUT_PLATFORM_DESTROY_THE_CANNONS, 0x45, KEY_FP_ITEM, "sea", "Room25", "TRES", 1, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x6B, 0x72, 0x41, 0x4F, 0x73, 0x00, 0x00}, 0x45, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // Abship/Room1.arc/Chest000
    {KeyKind::Dzx, PCR_LOC_SIX_EYE_REEF_SUBMARINE, 0x07, KEY_FP_ITEM, "Abship", "Room1", "TRES", 0, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {0x74, 0x61, 0x6B, 0x61, 0x72, 0x61, 0x33, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room1.arc/ScalableObject000
    {KeyKind::Dzx, PCR_LOC_FORSAKEN_FORTRESS_SECTOR_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room1", "SCOB", 0, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room1.arc/ScalableObject00A
    {KeyKind::Dzx, PCR_LOC_FORSAKEN_FORTRESS_SECTOR_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room1", "SCOB", 10, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room1.arc/ScalableObject00B
    {KeyKind::Dzx, PCR_LOC_FORSAKEN_FORTRESS_SECTOR_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room1", "SCOB", 11, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room1.arc/ScalableObject00C
    {KeyKind::Dzx, PCR_LOC_FORSAKEN_FORTRESS_SECTOR_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room1", "SCOB", 12, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room2.arc/ScalableObject00A
    {KeyKind::Dzx, PCR_LOC_STAR_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room2", "SCOB", 10, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room2.arc/ScalableObject00B
    {KeyKind::Dzx, PCR_LOC_STAR_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room2", "SCOB", 11, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room2.arc/ScalableObject00C
    {KeyKind::Dzx, PCR_LOC_STAR_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room2", "SCOB", 12, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room2.arc/ScalableObject00D
    {KeyKind::Dzx, PCR_LOC_STAR_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room2", "SCOB", 13, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room3.arc/ScalableObject006
    {KeyKind::Dzx, PCR_LOC_NORTHERN_FAIRY_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room3", "SCOB", 6, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room3.arc/ScalableObject007
    {KeyKind::Dzx, PCR_LOC_NORTHERN_FAIRY_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room3", "SCOB", 7, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room3.arc/ScalableObject008
    {KeyKind::Dzx, PCR_LOC_NORTHERN_FAIRY_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room3", "SCOB", 8, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room3.arc/ScalableObject009
    {KeyKind::Dzx, PCR_LOC_NORTHERN_FAIRY_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room3", "SCOB", 9, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room4.arc/ScalableObject007
    {KeyKind::Dzx, PCR_LOC_GALE_ISLE_SUNKEN_TREASURE, 0x62, KEY_FP_ITEM, "sea", "Room4", "SCOB", 7, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x62, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room4.arc/ScalableObject008
    {KeyKind::Dzx, PCR_LOC_GALE_ISLE_SUNKEN_TREASURE, 0x62, KEY_FP_ITEM, "sea", "Room4", "SCOB", 8, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x62, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room4.arc/ScalableObject009
    {KeyKind::Dzx, PCR_LOC_GALE_ISLE_SUNKEN_TREASURE, 0x62, KEY_FP_ITEM, "sea", "Room4", "SCOB", 9, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x62, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room4.arc/ScalableObject00A
    {KeyKind::Dzx, PCR_LOC_GALE_ISLE_SUNKEN_TREASURE, 0x62, KEY_FP_ITEM, "sea", "Room4", "SCOB", 10, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x62, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room5.arc/ScalableObject006
    {KeyKind::Dzx, PCR_LOC_CRESCENT_MOON_ISLAND_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room5", "SCOB", 6, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room5.arc/ScalableObject007
    {KeyKind::Dzx, PCR_LOC_CRESCENT_MOON_ISLAND_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room5", "SCOB", 7, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room5.arc/ScalableObject008
    {KeyKind::Dzx, PCR_LOC_CRESCENT_MOON_ISLAND_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room5", "SCOB", 8, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room5.arc/ScalableObject009
    {KeyKind::Dzx, PCR_LOC_CRESCENT_MOON_ISLAND_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room5", "SCOB", 9, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room6.arc/ScalableObject008
    {KeyKind::Dzx, PCR_LOC_SEVEN_STAR_ISLES_SUNKEN_TREASURE, 0x67, KEY_FP_ITEM, "sea", "Room6", "SCOB", 8, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x67, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room6.arc/ScalableObject009
    {KeyKind::Dzx, PCR_LOC_SEVEN_STAR_ISLES_SUNKEN_TREASURE, 0x67, KEY_FP_ITEM, "sea", "Room6", "SCOB", 9, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x67, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room6.arc/ScalableObject00A
    {KeyKind::Dzx, PCR_LOC_SEVEN_STAR_ISLES_SUNKEN_TREASURE, 0x67, KEY_FP_ITEM, "sea", "Room6", "SCOB", 10, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x67, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room6.arc/ScalableObject00B
    {KeyKind::Dzx, PCR_LOC_SEVEN_STAR_ISLES_SUNKEN_TREASURE, 0x67, KEY_FP_ITEM, "sea", "Room6", "SCOB", 11, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x67, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room7.arc/ScalableObject006
    {KeyKind::Dzx, PCR_LOC_OVERLOOK_ISLAND_SUNKEN_TREASURE, 0xC6, KEY_FP_ITEM, "sea", "Room7", "SCOB", 6, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0xC6, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room7.arc/ScalableObject007
    {KeyKind::Dzx, PCR_LOC_OVERLOOK_ISLAND_SUNKEN_TREASURE, 0xC6, KEY_FP_ITEM, "sea", "Room7", "SCOB", 7, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0xC6, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room7.arc/ScalableObject008
    {KeyKind::Dzx, PCR_LOC_OVERLOOK_ISLAND_SUNKEN_TREASURE, 0xC6, KEY_FP_ITEM, "sea", "Room7", "SCOB", 8, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0xC6, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room7.arc/ScalableObject009
    {KeyKind::Dzx, PCR_LOC_OVERLOOK_ISLAND_SUNKEN_TREASURE, 0xC6, KEY_FP_ITEM, "sea", "Room7", "SCOB", 9, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0xC6, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room8.arc/ScalableObject00E
    {KeyKind::Dzx, PCR_LOC_FOUR_EYE_REEF_SUNKEN_TREASURE, 0xC9, KEY_FP_ITEM, "sea", "Room8", "SCOB", 14, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0xC9, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room8.arc/ScalableObject00F
    {KeyKind::Dzx, PCR_LOC_FOUR_EYE_REEF_SUNKEN_TREASURE, 0xC9, KEY_FP_ITEM, "sea", "Room8", "SCOB", 15, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0xC9, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room8.arc/ScalableObject010
    {KeyKind::Dzx, PCR_LOC_FOUR_EYE_REEF_SUNKEN_TREASURE, 0xC9, KEY_FP_ITEM, "sea", "Room8", "SCOB", 16, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0xC9, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room8.arc/ScalableObject011
    {KeyKind::Dzx, PCR_LOC_FOUR_EYE_REEF_SUNKEN_TREASURE, 0xC9, KEY_FP_ITEM, "sea", "Room8", "SCOB", 17, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0xC9, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room9.arc/ScalableObject006
    {KeyKind::Dzx, PCR_LOC_MOTHER_AND_CHILD_ISLES_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room9", "SCOB", 6, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room9.arc/ScalableObject007
    {KeyKind::Dzx, PCR_LOC_MOTHER_AND_CHILD_ISLES_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room9", "SCOB", 7, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room9.arc/ScalableObject008
    {KeyKind::Dzx, PCR_LOC_MOTHER_AND_CHILD_ISLES_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room9", "SCOB", 8, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room9.arc/ScalableObject009
    {KeyKind::Dzx, PCR_LOC_MOTHER_AND_CHILD_ISLES_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room9", "SCOB", 9, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room10.arc/ScalableObject006
    {KeyKind::Dzx, PCR_LOC_SPECTACLE_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room10", "SCOB", 6, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room10.arc/ScalableObject007
    {KeyKind::Dzx, PCR_LOC_SPECTACLE_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room10", "SCOB", 7, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room10.arc/ScalableObject008
    {KeyKind::Dzx, PCR_LOC_SPECTACLE_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room10", "SCOB", 8, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room10.arc/ScalableObject009
    {KeyKind::Dzx, PCR_LOC_SPECTACLE_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room10", "SCOB", 9, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room11.arc/ScalableObject022
    {KeyKind::Dzx, PCR_LOC_WINDFALL_ISLAND_SUNKEN_TREASURE, 0x01, KEY_FP_ITEM, "sea", "Room11", "SCOB", 34, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x01, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room11.arc/ScalableObject023
    {KeyKind::Dzx, PCR_LOC_WINDFALL_ISLAND_SUNKEN_TREASURE, 0x01, KEY_FP_ITEM, "sea", "Room11", "SCOB", 35, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x01, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room11.arc/ScalableObject024
    {KeyKind::Dzx, PCR_LOC_WINDFALL_ISLAND_SUNKEN_TREASURE, 0x01, KEY_FP_ITEM, "sea", "Room11", "SCOB", 36, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x01, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room11.arc/ScalableObject025
    {KeyKind::Dzx, PCR_LOC_WINDFALL_ISLAND_SUNKEN_TREASURE, 0x01, KEY_FP_ITEM, "sea", "Room11", "SCOB", 37, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x01, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room12.arc/ScalableObject006
    {KeyKind::Dzx, PCR_LOC_PAWPRINT_ISLE_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room12", "SCOB", 6, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room12.arc/ScalableObject007
    {KeyKind::Dzx, PCR_LOC_PAWPRINT_ISLE_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room12", "SCOB", 7, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room12.arc/ScalableObject008
    {KeyKind::Dzx, PCR_LOC_PAWPRINT_ISLE_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room12", "SCOB", 8, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room12.arc/ScalableObject009
    {KeyKind::Dzx, PCR_LOC_PAWPRINT_ISLE_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room12", "SCOB", 9, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room13.arc/ScalableObject008
    {KeyKind::Dzx, PCR_LOC_DRAGON_ROOST_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room13", "SCOB", 8, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room13.arc/ScalableObject009
    {KeyKind::Dzx, PCR_LOC_DRAGON_ROOST_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room13", "SCOB", 9, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room13.arc/ScalableObject00A
    {KeyKind::Dzx, PCR_LOC_DRAGON_ROOST_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room13", "SCOB", 10, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room13.arc/ScalableObject00B
    {KeyKind::Dzx, PCR_LOC_DRAGON_ROOST_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room13", "SCOB", 11, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room14.arc/ScalableObject008
    {KeyKind::Dzx, PCR_LOC_FLIGHT_CONTROL_PLATFORM_SUNKEN_TREASURE, 0xC8, KEY_FP_ITEM, "sea", "Room14", "SCOB", 8, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0xC8, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room14.arc/ScalableObject009
    {KeyKind::Dzx, PCR_LOC_FLIGHT_CONTROL_PLATFORM_SUNKEN_TREASURE, 0xC8, KEY_FP_ITEM, "sea", "Room14", "SCOB", 9, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0xC8, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room14.arc/ScalableObject00A
    {KeyKind::Dzx, PCR_LOC_FLIGHT_CONTROL_PLATFORM_SUNKEN_TREASURE, 0xC8, KEY_FP_ITEM, "sea", "Room14", "SCOB", 10, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0xC8, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room14.arc/ScalableObject00B
    {KeyKind::Dzx, PCR_LOC_FLIGHT_CONTROL_PLATFORM_SUNKEN_TREASURE, 0xC8, KEY_FP_ITEM, "sea", "Room14", "SCOB", 11, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0xC8, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room15.arc/ScalableObject007
    {KeyKind::Dzx, PCR_LOC_WESTERN_FAIRY_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room15", "SCOB", 7, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room15.arc/ScalableObject008
    {KeyKind::Dzx, PCR_LOC_WESTERN_FAIRY_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room15", "SCOB", 8, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room15.arc/ScalableObject009
    {KeyKind::Dzx, PCR_LOC_WESTERN_FAIRY_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room15", "SCOB", 9, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room15.arc/ScalableObject00A
    {KeyKind::Dzx, PCR_LOC_WESTERN_FAIRY_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room15", "SCOB", 10, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room16.arc/ScalableObject008
    {KeyKind::Dzx, PCR_LOC_ROCK_SPIRE_ISLE_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room16", "SCOB", 8, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room16.arc/ScalableObject009
    {KeyKind::Dzx, PCR_LOC_ROCK_SPIRE_ISLE_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room16", "SCOB", 9, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room16.arc/ScalableObject00A
    {KeyKind::Dzx, PCR_LOC_ROCK_SPIRE_ISLE_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room16", "SCOB", 10, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room16.arc/ScalableObject00B
    {KeyKind::Dzx, PCR_LOC_ROCK_SPIRE_ISLE_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room16", "SCOB", 11, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room17.arc/ScalableObject00B
    {KeyKind::Dzx, PCR_LOC_TINGLE_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room17", "SCOB", 11, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room17.arc/ScalableObject00C
    {KeyKind::Dzx, PCR_LOC_TINGLE_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room17", "SCOB", 12, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room17.arc/ScalableObject00D
    {KeyKind::Dzx, PCR_LOC_TINGLE_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room17", "SCOB", 13, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room17.arc/ScalableObject00E
    {KeyKind::Dzx, PCR_LOC_TINGLE_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room17", "SCOB", 14, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room18.arc/ScalableObject006
    {KeyKind::Dzx, PCR_LOC_NORTHERN_TRIANGLE_ISLAND_SUNKEN_TREASURE, 0xCA, KEY_FP_ITEM, "sea", "Room18", "SCOB", 6, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0xCA, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room18.arc/ScalableObject007
    {KeyKind::Dzx, PCR_LOC_NORTHERN_TRIANGLE_ISLAND_SUNKEN_TREASURE, 0xCA, KEY_FP_ITEM, "sea", "Room18", "SCOB", 7, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0xCA, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room18.arc/ScalableObject008
    {KeyKind::Dzx, PCR_LOC_NORTHERN_TRIANGLE_ISLAND_SUNKEN_TREASURE, 0xCA, KEY_FP_ITEM, "sea", "Room18", "SCOB", 8, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0xCA, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room18.arc/ScalableObject009
    {KeyKind::Dzx, PCR_LOC_NORTHERN_TRIANGLE_ISLAND_SUNKEN_TREASURE, 0xCA, KEY_FP_ITEM, "sea", "Room18", "SCOB", 9, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0xCA, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room19.arc/ScalableObject006
    {KeyKind::Dzx, PCR_LOC_EASTERN_FAIRY_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room19", "SCOB", 6, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room19.arc/ScalableObject007
    {KeyKind::Dzx, PCR_LOC_EASTERN_FAIRY_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room19", "SCOB", 7, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room19.arc/ScalableObject008
    {KeyKind::Dzx, PCR_LOC_EASTERN_FAIRY_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room19", "SCOB", 8, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room19.arc/ScalableObject009
    {KeyKind::Dzx, PCR_LOC_EASTERN_FAIRY_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room19", "SCOB", 9, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room20.arc/ScalableObject00B
    {KeyKind::Dzx, PCR_LOC_FIRE_MOUNTAIN_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room20", "SCOB", 11, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room20.arc/ScalableObject00C
    {KeyKind::Dzx, PCR_LOC_FIRE_MOUNTAIN_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room20", "SCOB", 12, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room20.arc/ScalableObject00D
    {KeyKind::Dzx, PCR_LOC_FIRE_MOUNTAIN_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room20", "SCOB", 13, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room20.arc/ScalableObject00E
    {KeyKind::Dzx, PCR_LOC_FIRE_MOUNTAIN_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room20", "SCOB", 14, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room21.arc/ScalableObject009
    {KeyKind::Dzx, PCR_LOC_STAR_BELT_ARCHIPELAGO_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room21", "SCOB", 9, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room21.arc/ScalableObject00A
    {KeyKind::Dzx, PCR_LOC_STAR_BELT_ARCHIPELAGO_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room21", "SCOB", 10, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room21.arc/ScalableObject00B
    {KeyKind::Dzx, PCR_LOC_STAR_BELT_ARCHIPELAGO_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room21", "SCOB", 11, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room21.arc/ScalableObject00C
    {KeyKind::Dzx, PCR_LOC_STAR_BELT_ARCHIPELAGO_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room21", "SCOB", 12, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room22.arc/ScalableObject007
    {KeyKind::Dzx, PCR_LOC_THREE_EYE_REEF_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room22", "SCOB", 7, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room22.arc/ScalableObject008
    {KeyKind::Dzx, PCR_LOC_THREE_EYE_REEF_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room22", "SCOB", 8, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room22.arc/ScalableObject009
    {KeyKind::Dzx, PCR_LOC_THREE_EYE_REEF_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room22", "SCOB", 9, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room22.arc/ScalableObject00A
    {KeyKind::Dzx, PCR_LOC_THREE_EYE_REEF_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room22", "SCOB", 10, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room23.arc/ScalableObject005
    {KeyKind::Dzx, PCR_LOC_GREATFISH_ISLE_SUNKEN_TREASURE, 0x61, KEY_FP_ITEM, "sea", "Room23", "SCOB", 5, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x61, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room23.arc/ScalableObject006
    {KeyKind::Dzx, PCR_LOC_GREATFISH_ISLE_SUNKEN_TREASURE, 0x61, KEY_FP_ITEM, "sea", "Room23", "SCOB", 6, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x61, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room23.arc/ScalableObject007
    {KeyKind::Dzx, PCR_LOC_GREATFISH_ISLE_SUNKEN_TREASURE, 0x61, KEY_FP_ITEM, "sea", "Room23", "SCOB", 7, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x61, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room23.arc/ScalableObject008
    {KeyKind::Dzx, PCR_LOC_GREATFISH_ISLE_SUNKEN_TREASURE, 0x61, KEY_FP_ITEM, "sea", "Room23", "SCOB", 8, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x61, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room24.arc/ScalableObject00C
    {KeyKind::Dzx, PCR_LOC_CYCLOPS_REEF_SUNKEN_TREASURE, 0xC5, KEY_FP_ITEM, "sea", "Room24", "SCOB", 12, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0xC5, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room24.arc/ScalableObject00D
    {KeyKind::Dzx, PCR_LOC_CYCLOPS_REEF_SUNKEN_TREASURE, 0xC5, KEY_FP_ITEM, "sea", "Room24", "SCOB", 13, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0xC5, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room24.arc/ScalableObject00E
    {KeyKind::Dzx, PCR_LOC_CYCLOPS_REEF_SUNKEN_TREASURE, 0xC5, KEY_FP_ITEM, "sea", "Room24", "SCOB", 14, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0xC5, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room24.arc/ScalableObject00F
    {KeyKind::Dzx, PCR_LOC_CYCLOPS_REEF_SUNKEN_TREASURE, 0xC5, KEY_FP_ITEM, "sea", "Room24", "SCOB", 15, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0xC5, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room25.arc/ScalableObject007
    {KeyKind::Dzx, PCR_LOC_SIX_EYE_REEF_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room25", "SCOB", 7, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room25.arc/ScalableObject008
    {KeyKind::Dzx, PCR_LOC_SIX_EYE_REEF_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room25", "SCOB", 8, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room25.arc/ScalableObject009
    {KeyKind::Dzx, PCR_LOC_SIX_EYE_REEF_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room25", "SCOB", 9, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room25.arc/ScalableObject00A
    {KeyKind::Dzx, PCR_LOC_SIX_EYE_REEF_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room25", "SCOB", 10, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room26.arc/ScalableObject006
    {KeyKind::Dzx, PCR_LOC_TOWER_OF_THE_GODS_SECTOR_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room26", "SCOB", 6, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room26.arc/ScalableObject007
    {KeyKind::Dzx, PCR_LOC_TOWER_OF_THE_GODS_SECTOR_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room26", "SCOB", 7, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room26.arc/ScalableObject008
    {KeyKind::Dzx, PCR_LOC_TOWER_OF_THE_GODS_SECTOR_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room26", "SCOB", 8, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room26.arc/ScalableObject009
    {KeyKind::Dzx, PCR_LOC_TOWER_OF_THE_GODS_SECTOR_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room26", "SCOB", 9, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room27.arc/ScalableObject006
    {KeyKind::Dzx, PCR_LOC_EASTERN_TRIANGLE_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room27", "SCOB", 6, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room27.arc/ScalableObject007
    {KeyKind::Dzx, PCR_LOC_EASTERN_TRIANGLE_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room27", "SCOB", 7, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room27.arc/ScalableObject008
    {KeyKind::Dzx, PCR_LOC_EASTERN_TRIANGLE_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room27", "SCOB", 8, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room27.arc/ScalableObject009
    {KeyKind::Dzx, PCR_LOC_EASTERN_TRIANGLE_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room27", "SCOB", 9, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room28.arc/ScalableObject006
    {KeyKind::Dzx, PCR_LOC_THORNED_FAIRY_ISLAND_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room28", "SCOB", 6, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room28.arc/ScalableObject007
    {KeyKind::Dzx, PCR_LOC_THORNED_FAIRY_ISLAND_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room28", "SCOB", 7, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room28.arc/ScalableObject008
    {KeyKind::Dzx, PCR_LOC_THORNED_FAIRY_ISLAND_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room28", "SCOB", 8, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room28.arc/ScalableObject009
    {KeyKind::Dzx, PCR_LOC_THORNED_FAIRY_ISLAND_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room28", "SCOB", 9, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room29.arc/ScalableObject006
    {KeyKind::Dzx, PCR_LOC_NEEDLE_ROCK_ISLE_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room29", "SCOB", 6, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room29.arc/ScalableObject007
    {KeyKind::Dzx, PCR_LOC_NEEDLE_ROCK_ISLE_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room29", "SCOB", 7, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room29.arc/ScalableObject008
    {KeyKind::Dzx, PCR_LOC_NEEDLE_ROCK_ISLE_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room29", "SCOB", 8, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room29.arc/ScalableObject009
    {KeyKind::Dzx, PCR_LOC_NEEDLE_ROCK_ISLE_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room29", "SCOB", 9, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room30.arc/ScalableObject007
    {KeyKind::Dzx, PCR_LOC_ISLET_OF_STEEL_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room30", "SCOB", 7, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room30.arc/ScalableObject008
    {KeyKind::Dzx, PCR_LOC_ISLET_OF_STEEL_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room30", "SCOB", 8, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room30.arc/ScalableObject009
    {KeyKind::Dzx, PCR_LOC_ISLET_OF_STEEL_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room30", "SCOB", 9, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room30.arc/ScalableObject00A
    {KeyKind::Dzx, PCR_LOC_ISLET_OF_STEEL_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room30", "SCOB", 10, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room31.arc/ScalableObject006
    {KeyKind::Dzx, PCR_LOC_STONE_WATCHER_ISLAND_SUNKEN_TREASURE, 0x63, KEY_FP_ITEM, "sea", "Room31", "SCOB", 6, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x63, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room31.arc/ScalableObject007
    {KeyKind::Dzx, PCR_LOC_STONE_WATCHER_ISLAND_SUNKEN_TREASURE, 0x63, KEY_FP_ITEM, "sea", "Room31", "SCOB", 7, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x63, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room31.arc/ScalableObject008
    {KeyKind::Dzx, PCR_LOC_STONE_WATCHER_ISLAND_SUNKEN_TREASURE, 0x63, KEY_FP_ITEM, "sea", "Room31", "SCOB", 8, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x63, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room31.arc/ScalableObject009
    {KeyKind::Dzx, PCR_LOC_STONE_WATCHER_ISLAND_SUNKEN_TREASURE, 0x63, KEY_FP_ITEM, "sea", "Room31", "SCOB", 9, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x63, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room32.arc/ScalableObject006
    {KeyKind::Dzx, PCR_LOC_SOUTHERN_TRIANGLE_ISLAND_SUNKEN_TREASURE, 0x66, KEY_FP_ITEM, "sea", "Room32", "SCOB", 6, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x66, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room32.arc/ScalableObject007
    {KeyKind::Dzx, PCR_LOC_SOUTHERN_TRIANGLE_ISLAND_SUNKEN_TREASURE, 0x66, KEY_FP_ITEM, "sea", "Room32", "SCOB", 7, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x66, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room32.arc/ScalableObject008
    {KeyKind::Dzx, PCR_LOC_SOUTHERN_TRIANGLE_ISLAND_SUNKEN_TREASURE, 0x66, KEY_FP_ITEM, "sea", "Room32", "SCOB", 8, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x66, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room32.arc/ScalableObject009
    {KeyKind::Dzx, PCR_LOC_SOUTHERN_TRIANGLE_ISLAND_SUNKEN_TREASURE, 0x66, KEY_FP_ITEM, "sea", "Room32", "SCOB", 9, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x66, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room33.arc/ScalableObject008
    {KeyKind::Dzx, PCR_LOC_PRIVATE_OASIS_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room33", "SCOB", 8, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room33.arc/ScalableObject009
    {KeyKind::Dzx, PCR_LOC_PRIVATE_OASIS_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room33", "SCOB", 9, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room33.arc/ScalableObject00A
    {KeyKind::Dzx, PCR_LOC_PRIVATE_OASIS_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room33", "SCOB", 10, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room33.arc/ScalableObject00B
    {KeyKind::Dzx, PCR_LOC_PRIVATE_OASIS_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room33", "SCOB", 11, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room34.arc/ScalableObject006
    {KeyKind::Dzx, PCR_LOC_BOMB_ISLAND_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room34", "SCOB", 6, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room34.arc/ScalableObject007
    {KeyKind::Dzx, PCR_LOC_BOMB_ISLAND_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room34", "SCOB", 7, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room34.arc/ScalableObject008
    {KeyKind::Dzx, PCR_LOC_BOMB_ISLAND_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room34", "SCOB", 8, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room34.arc/ScalableObject009
    {KeyKind::Dzx, PCR_LOC_BOMB_ISLAND_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room34", "SCOB", 9, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room35.arc/ScalableObject007
    {KeyKind::Dzx, PCR_LOC_BIRDS_PEAK_ROCK_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room35", "SCOB", 7, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room35.arc/ScalableObject008
    {KeyKind::Dzx, PCR_LOC_BIRDS_PEAK_ROCK_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room35", "SCOB", 8, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room35.arc/ScalableObject009
    {KeyKind::Dzx, PCR_LOC_BIRDS_PEAK_ROCK_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room35", "SCOB", 9, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room35.arc/ScalableObject00A
    {KeyKind::Dzx, PCR_LOC_BIRDS_PEAK_ROCK_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room35", "SCOB", 10, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room36.arc/ScalableObject009
    {KeyKind::Dzx, PCR_LOC_DIAMOND_STEPPE_ISLAND_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room36", "SCOB", 9, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room36.arc/ScalableObject00A
    {KeyKind::Dzx, PCR_LOC_DIAMOND_STEPPE_ISLAND_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room36", "SCOB", 10, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room36.arc/ScalableObject00B
    {KeyKind::Dzx, PCR_LOC_DIAMOND_STEPPE_ISLAND_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room36", "SCOB", 11, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room36.arc/ScalableObject00C
    {KeyKind::Dzx, PCR_LOC_DIAMOND_STEPPE_ISLAND_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room36", "SCOB", 12, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room37.arc/ScalableObject007
    {KeyKind::Dzx, PCR_LOC_FIVE_EYE_REEF_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room37", "SCOB", 7, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room37.arc/ScalableObject008
    {KeyKind::Dzx, PCR_LOC_FIVE_EYE_REEF_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room37", "SCOB", 8, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room37.arc/ScalableObject009
    {KeyKind::Dzx, PCR_LOC_FIVE_EYE_REEF_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room37", "SCOB", 9, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room37.arc/ScalableObject00A
    {KeyKind::Dzx, PCR_LOC_FIVE_EYE_REEF_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room37", "SCOB", 10, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room38.arc/ScalableObject00D
    {KeyKind::Dzx, PCR_LOC_SHARK_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room38", "SCOB", 13, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room38.arc/ScalableObject00E
    {KeyKind::Dzx, PCR_LOC_SHARK_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room38", "SCOB", 14, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room38.arc/ScalableObject00F
    {KeyKind::Dzx, PCR_LOC_SHARK_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room38", "SCOB", 15, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room38.arc/ScalableObject010
    {KeyKind::Dzx, PCR_LOC_SHARK_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room38", "SCOB", 16, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room39.arc/ScalableObject006
    {KeyKind::Dzx, PCR_LOC_SOUTHERN_FAIRY_ISLAND_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room39", "SCOB", 6, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room39.arc/ScalableObject007
    {KeyKind::Dzx, PCR_LOC_SOUTHERN_FAIRY_ISLAND_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room39", "SCOB", 7, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room39.arc/ScalableObject008
    {KeyKind::Dzx, PCR_LOC_SOUTHERN_FAIRY_ISLAND_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room39", "SCOB", 8, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room39.arc/ScalableObject009
    {KeyKind::Dzx, PCR_LOC_SOUTHERN_FAIRY_ISLAND_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room39", "SCOB", 9, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room40.arc/ScalableObject00A
    {KeyKind::Dzx, PCR_LOC_ICE_RING_ISLE_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room40", "SCOB", 10, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room40.arc/ScalableObject00B
    {KeyKind::Dzx, PCR_LOC_ICE_RING_ISLE_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room40", "SCOB", 11, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room40.arc/ScalableObject00C
    {KeyKind::Dzx, PCR_LOC_ICE_RING_ISLE_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room40", "SCOB", 12, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room40.arc/ScalableObject00D
    {KeyKind::Dzx, PCR_LOC_ICE_RING_ISLE_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room40", "SCOB", 13, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room41.arc/ScalableObject014
    {KeyKind::Dzx, PCR_LOC_FOREST_HAVEN_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room41", "SCOB", 20, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room41.arc/ScalableObject015
    {KeyKind::Dzx, PCR_LOC_FOREST_HAVEN_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room41", "SCOB", 21, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room41.arc/ScalableObject016
    {KeyKind::Dzx, PCR_LOC_FOREST_HAVEN_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room41", "SCOB", 22, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room41.arc/ScalableObject017
    {KeyKind::Dzx, PCR_LOC_FOREST_HAVEN_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room41", "SCOB", 23, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room42.arc/ScalableObject006
    {KeyKind::Dzx, PCR_LOC_CLIFF_PLATEAU_ISLES_SUNKEN_TREASURE, 0x65, KEY_FP_ITEM, "sea", "Room42", "SCOB", 6, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x65, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room42.arc/ScalableObject007
    {KeyKind::Dzx, PCR_LOC_CLIFF_PLATEAU_ISLES_SUNKEN_TREASURE, 0x65, KEY_FP_ITEM, "sea", "Room42", "SCOB", 7, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x65, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room42.arc/ScalableObject008
    {KeyKind::Dzx, PCR_LOC_CLIFF_PLATEAU_ISLES_SUNKEN_TREASURE, 0x65, KEY_FP_ITEM, "sea", "Room42", "SCOB", 8, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x65, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room42.arc/ScalableObject009
    {KeyKind::Dzx, PCR_LOC_CLIFF_PLATEAU_ISLES_SUNKEN_TREASURE, 0x65, KEY_FP_ITEM, "sea", "Room42", "SCOB", 9, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x65, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room43.arc/ScalableObject007
    {KeyKind::Dzx, PCR_LOC_HORSESHOE_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room43", "SCOB", 7, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room43.arc/ScalableObject008
    {KeyKind::Dzx, PCR_LOC_HORSESHOE_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room43", "SCOB", 8, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room43.arc/ScalableObject009
    {KeyKind::Dzx, PCR_LOC_HORSESHOE_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room43", "SCOB", 9, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room43.arc/ScalableObject00A
    {KeyKind::Dzx, PCR_LOC_HORSESHOE_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room43", "SCOB", 10, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room44.arc/ScalableObject013
    {KeyKind::Dzx, PCR_LOC_OUTSET_ISLAND_SUNKEN_TREASURE, 0x64, KEY_FP_ITEM, "sea", "Room44", "SCOB", 19, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x64, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room44.arc/ScalableObject014
    {KeyKind::Dzx, PCR_LOC_OUTSET_ISLAND_SUNKEN_TREASURE, 0x64, KEY_FP_ITEM, "sea", "Room44", "SCOB", 20, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x64, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room44.arc/ScalableObject015
    {KeyKind::Dzx, PCR_LOC_OUTSET_ISLAND_SUNKEN_TREASURE, 0x64, KEY_FP_ITEM, "sea", "Room44", "SCOB", 21, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x64, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room44.arc/ScalableObject016
    {KeyKind::Dzx, PCR_LOC_OUTSET_ISLAND_SUNKEN_TREASURE, 0x64, KEY_FP_ITEM, "sea", "Room44", "SCOB", 22, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x64, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room45.arc/ScalableObject026
    {KeyKind::Dzx, PCR_LOC_HEADSTONE_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room45", "SCOB", 38, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room45.arc/ScalableObject027
    {KeyKind::Dzx, PCR_LOC_HEADSTONE_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room45", "SCOB", 39, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room45.arc/ScalableObject028
    {KeyKind::Dzx, PCR_LOC_HEADSTONE_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room45", "SCOB", 40, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room45.arc/ScalableObject029
    {KeyKind::Dzx, PCR_LOC_HEADSTONE_ISLAND_SUNKEN_TREASURE, 0x0F, KEY_FP_ITEM, "sea", "Room45", "SCOB", 41, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x0F, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room46.arc/ScalableObject008
    {KeyKind::Dzx, PCR_LOC_TWO_EYE_REEF_SUNKEN_TREASURE, 0x68, KEY_FP_ITEM, "sea", "Room46", "SCOB", 8, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x68, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room46.arc/ScalableObject009
    {KeyKind::Dzx, PCR_LOC_TWO_EYE_REEF_SUNKEN_TREASURE, 0x68, KEY_FP_ITEM, "sea", "Room46", "SCOB", 9, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x68, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room46.arc/ScalableObject00A
    {KeyKind::Dzx, PCR_LOC_TWO_EYE_REEF_SUNKEN_TREASURE, 0x68, KEY_FP_ITEM, "sea", "Room46", "SCOB", 10, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x68, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room46.arc/ScalableObject00B
    {KeyKind::Dzx, PCR_LOC_TWO_EYE_REEF_SUNKEN_TREASURE, 0x68, KEY_FP_ITEM, "sea", "Room46", "SCOB", 11, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x68, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room47.arc/ScalableObject006
    {KeyKind::Dzx, PCR_LOC_ANGULAR_ISLES_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room47", "SCOB", 6, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room47.arc/ScalableObject007
    {KeyKind::Dzx, PCR_LOC_ANGULAR_ISLES_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room47", "SCOB", 7, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room47.arc/ScalableObject008
    {KeyKind::Dzx, PCR_LOC_ANGULAR_ISLES_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room47", "SCOB", 8, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room47.arc/ScalableObject009
    {KeyKind::Dzx, PCR_LOC_ANGULAR_ISLES_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room47", "SCOB", 9, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room48.arc/ScalableObject006
    {KeyKind::Dzx, PCR_LOC_BOATING_COURSE_SUNKEN_TREASURE, 0xC7, KEY_FP_ITEM, "sea", "Room48", "SCOB", 6, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0xC7, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room48.arc/ScalableObject007
    {KeyKind::Dzx, PCR_LOC_BOATING_COURSE_SUNKEN_TREASURE, 0xC7, KEY_FP_ITEM, "sea", "Room48", "SCOB", 7, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0xC7, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room48.arc/ScalableObject008
    {KeyKind::Dzx, PCR_LOC_BOATING_COURSE_SUNKEN_TREASURE, 0xC7, KEY_FP_ITEM, "sea", "Room48", "SCOB", 8, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0xC7, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room48.arc/ScalableObject009
    {KeyKind::Dzx, PCR_LOC_BOATING_COURSE_SUNKEN_TREASURE, 0xC7, KEY_FP_ITEM, "sea", "Room48", "SCOB", 9, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0xC7, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room49.arc/ScalableObject006
    {KeyKind::Dzx, PCR_LOC_FIVE_STAR_ISLES_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room49", "SCOB", 6, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room49.arc/ScalableObject007
    {KeyKind::Dzx, PCR_LOC_FIVE_STAR_ISLES_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room49", "SCOB", 7, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room49.arc/ScalableObject008
    {KeyKind::Dzx, PCR_LOC_FIVE_STAR_ISLES_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room49", "SCOB", 8, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
    // sea/Room49.arc/ScalableObject009
    {KeyKind::Dzx, PCR_LOC_FIVE_STAR_ISLES_SUNKEN_TREASURE, 0x07, KEY_FP_ITEM, "sea", "Room49", "SCOB", 9, DzxField::SalvageItem, 0x08, 4, 0x00000FF0u, {0x53, 0x61, 0x6C, 0x76, 0x61, 0x67, 0x65, 0x00}, 0x07, nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE},
};

// Per location: PCR_LOC_* name (for tests and logs) and class bits.
static const LocInfo kLocInfo[] = {
    {"PCR_LOC_OUTSET_ISLAND_UNDERNEATH_LINKS_HOUSE", PCR_CLASS_CHEST},
    {"PCR_LOC_OUTSET_ISLAND_MESA_THE_GRASSCUTTERS_HOUSE", PCR_CLASS_CHEST},
    {"PCR_LOC_OUTSET_ISLAND_ORCA_GIVE_10_KNIGHTS_CRESTS", PCR_CLASS_EVENT | PCR_CLASS_CODE},
    {"PCR_LOC_OUTSET_ISLAND_ORCA_HIT_500_TIMES", PCR_CLASS_CODE},
    {"PCR_LOC_OUTSET_ISLAND_GREAT_FAIRY", PCR_CLASS_CODE},
    {"PCR_LOC_OUTSET_ISLAND_JABUNS_CAVE", PCR_CLASS_ADDED | PCR_CLASS_CODE},
    {"PCR_LOC_OUTSET_ISLAND_DIG_UP_BLACK_SOIL", PCR_CLASS_SCOB},
    {"PCR_LOC_OUTSET_ISLAND_SAVAGE_LABYRINTH_FLOOR_30", PCR_CLASS_CHEST},
    {"PCR_LOC_OUTSET_ISLAND_SAVAGE_LABYRINTH_FLOOR_50", PCR_CLASS_CHEST},
    {"PCR_LOC_WINDFALL_ISLAND_JAIL_TINGLE_FIRST_GIFT", PCR_CLASS_CODE},
    {"PCR_LOC_WINDFALL_ISLAND_JAIL_TINGLE_SECOND_GIFT", PCR_CLASS_CODE},
    {"PCR_LOC_WINDFALL_ISLAND_JAIL_MAZE_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_WINDFALL_ISLAND_CHU_JELLY_JUICE_SHOP_GIVE_15_GREEN_CHU_JELLY", PCR_CLASS_CUSTOM},
    {"PCR_LOC_WINDFALL_ISLAND_CHU_JELLY_JUICE_SHOP_GIVE_15_BLUE_CHU_JELLY", PCR_CLASS_CUSTOM},
    {"PCR_LOC_WINDFALL_ISLAND_IVAN_CATCH_KILLER_BEES", PCR_CLASS_CODE},
    {"PCR_LOC_WINDFALL_ISLAND_MRS_MARIE_CATCH_KILLER_BEES", PCR_CLASS_CODE},
    {"PCR_LOC_WINDFALL_ISLAND_MRS_MARIE_GIVE_1_JOY_PENDANT", PCR_CLASS_CODE},
    {"PCR_LOC_WINDFALL_ISLAND_MRS_MARIE_GIVE_21_JOY_PENDANTS", PCR_CLASS_CODE},
    {"PCR_LOC_WINDFALL_ISLAND_MRS_MARIE_GIVE_40_JOY_PENDANTS", PCR_CLASS_CODE},
    {"PCR_LOC_WINDFALL_ISLAND_LENZOS_HOUSE_LEFT_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_WINDFALL_ISLAND_LENZOS_HOUSE_RIGHT_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_WINDFALL_ISLAND_LENZOS_HOUSE_BECOME_LENZOS_ASSISTANT", PCR_CLASS_CODE},
    {"PCR_LOC_WINDFALL_ISLAND_LENZOS_HOUSE_BRING_FOREST_FIREFLY", PCR_CLASS_CODE},
    {"PCR_LOC_WINDFALL_ISLAND_HOUSE_OF_WEALTH_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_WINDFALL_ISLAND_MAGGIES_FATHER_GIVE_20_SKULL_NECKLACES", PCR_CLASS_EVENT},
    {"PCR_LOC_WINDFALL_ISLAND_MAGGIE_FREE_ITEM", PCR_CLASS_CODE},
    {"PCR_LOC_WINDFALL_ISLAND_MAGGIE_DELIVERY_REWARD", PCR_CLASS_CODE},
    {"PCR_LOC_WINDFALL_ISLAND_CAFE_BAR_POSTMAN", PCR_CLASS_EVENT},
    {"PCR_LOC_WINDFALL_ISLAND_KREEB_LIGHT_UP_LIGHTHOUSE", PCR_CLASS_CODE},
    {"PCR_LOC_WINDFALL_ISLAND_TRANSPARENT_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_WINDFALL_ISLAND_TOTT_TEACH_RHYTHM", PCR_CLASS_EVENT},
    {"PCR_LOC_WINDFALL_ISLAND_PIRATE_SHIP", PCR_CLASS_CHEST},
    {"PCR_LOC_WINDFALL_ISLAND_5_RUPEE_AUCTION", PCR_CLASS_CODE},
    {"PCR_LOC_WINDFALL_ISLAND_40_RUPEE_AUCTION", PCR_CLASS_CODE},
    {"PCR_LOC_WINDFALL_ISLAND_60_RUPEE_AUCTION", PCR_CLASS_CODE},
    {"PCR_LOC_WINDFALL_ISLAND_80_RUPEE_AUCTION", PCR_CLASS_CODE},
    {"PCR_LOC_WINDFALL_ISLAND_ZUNARI_STOCK_EXOTIC_FLOWER_IN_ZUNARIS_SHOP", PCR_CLASS_CODE | PCR_CLASS_CUSTOM},
    {"PCR_LOC_WINDFALL_ISLAND_SAM_DECORATE_THE_TOWN", PCR_CLASS_CODE},
    {"PCR_LOC_WINDFALL_ISLAND_KANE_PLACE_SHOP_GURU_STATUE_ON_GATE", PCR_CLASS_CODE},
    {"PCR_LOC_WINDFALL_ISLAND_KANE_PLACE_POSTMAN_STATUE_ON_GATE", PCR_CLASS_CODE},
    {"PCR_LOC_WINDFALL_ISLAND_KANE_PLACE_SIX_FLAGS_ON_GATE", PCR_CLASS_CODE},
    {"PCR_LOC_WINDFALL_ISLAND_KANE_PLACE_SIX_IDOLS_ON_GATE", PCR_CLASS_CODE},
    {"PCR_LOC_WINDFALL_ISLAND_MILA_FOLLOW_THE_THIEF", PCR_CLASS_EVENT},
    {"PCR_LOC_WINDFALL_ISLAND_BATTLESQUID_FIRST_PRIZE", PCR_CLASS_CODE},
    {"PCR_LOC_WINDFALL_ISLAND_BATTLESQUID_SECOND_PRIZE", PCR_CLASS_CODE},
    {"PCR_LOC_WINDFALL_ISLAND_BATTLESQUID_UNDER_20_SHOTS_PRIZE", PCR_CLASS_CODE},
    {"PCR_LOC_WINDFALL_ISLAND_POMPIE_AND_VERA_SECRET_MEETING_PHOTO", PCR_CLASS_CODE},
    {"PCR_LOC_WINDFALL_ISLAND_KAMO_FULL_MOON_PHOTO", PCR_CLASS_CODE},
    {"PCR_LOC_WINDFALL_ISLAND_MINENCO_MISS_WINDFALL_PHOTO", PCR_CLASS_CODE},
    {"PCR_LOC_WINDFALL_ISLAND_LINDA_AND_ANTON", PCR_CLASS_CODE},
    {"PCR_LOC_DRAGON_ROOST_ISLAND_WIND_SHRINE", PCR_CLASS_EVENT},
    {"PCR_LOC_DRAGON_ROOST_ISLAND_RITO_AERIE_GIVE_HOSKIT_20_GOLDEN_FEATHERS", PCR_CLASS_EVENT},
    {"PCR_LOC_DRAGON_ROOST_ISLAND_CHEST_ON_TOP_OF_BOULDER", PCR_CLASS_CHEST},
    {"PCR_LOC_DRAGON_ROOST_ISLAND_FLY_ACROSS_PLATFORMS_AROUND_ISLAND", PCR_CLASS_CHEST},
    {"PCR_LOC_DRAGON_ROOST_ISLAND_RITO_AERIE_MAIL_SORTING", PCR_CLASS_EVENT},
    {"PCR_LOC_DRAGON_ROOST_ISLAND_SECRET_CAVE", PCR_CLASS_CHEST},
    {"PCR_LOC_DRAGON_ROOST_CAVERN_FIRST_ROOM", PCR_CLASS_CHEST},
    {"PCR_LOC_DRAGON_ROOST_CAVERN_ALCOVE_WITH_WATER_JUGS", PCR_CLASS_CHEST},
    {"PCR_LOC_DRAGON_ROOST_CAVERN_WATER_JUG_ON_UPPER_SHELF", PCR_CLASS_ACTR},
    {"PCR_LOC_DRAGON_ROOST_CAVERN_BOARDED_UP_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_DRAGON_ROOST_CAVERN_CHEST_ACROSS_LAVA_PIT", PCR_CLASS_CHEST},
    {"PCR_LOC_DRAGON_ROOST_CAVERN_RAT_ROOM", PCR_CLASS_CHEST},
    {"PCR_LOC_DRAGON_ROOST_CAVERN_RAT_ROOM_BOARDED_UP_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_DRAGON_ROOST_CAVERN_BIRDS_NEST", PCR_CLASS_ACTR},
    {"PCR_LOC_DRAGON_ROOST_CAVERN_DARK_ROOM", PCR_CLASS_CHEST},
    {"PCR_LOC_DRAGON_ROOST_CAVERN_TINGLE_CHEST_IN_HUB_ROOM", PCR_CLASS_CHEST},
    {"PCR_LOC_DRAGON_ROOST_CAVERN_POT_ON_UPPER_SHELF_IN_POT_ROOM", PCR_CLASS_ACTR},
    {"PCR_LOC_DRAGON_ROOST_CAVERN_POT_ROOM_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_DRAGON_ROOST_CAVERN_MINIBOSS", PCR_CLASS_ADDED | PCR_CLASS_CODE},
    {"PCR_LOC_DRAGON_ROOST_CAVERN_UNDER_ROPE_BRIDGE", PCR_CLASS_CHEST},
    {"PCR_LOC_DRAGON_ROOST_CAVERN_TINGLE_STATUE_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_DRAGON_ROOST_CAVERN_BIG_KEY_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_DRAGON_ROOST_CAVERN_BOSS_STAIRS_RIGHT_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_DRAGON_ROOST_CAVERN_BOSS_STAIRS_LEFT_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_DRAGON_ROOST_CAVERN_BOSS_STAIRS_RIGHT_POT", PCR_CLASS_ACTR},
    {"PCR_LOC_DRAGON_ROOST_CAVERN_GOHMA_HEART_CONTAINER", PCR_CLASS_ACTR | PCR_CLASS_CODE},
    {"PCR_LOC_FOREST_HAVEN_ON_TREE_BRANCH", PCR_CLASS_ACTR | PCR_CLASS_CODE},
    {"PCR_LOC_FOREST_HAVEN_SMALL_ISLAND_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_FORBIDDEN_WOODS_FIRST_ROOM", PCR_CLASS_CHEST},
    {"PCR_LOC_FORBIDDEN_WOODS_INSIDE_HOLLOW_TREES_MOUTH", PCR_CLASS_CHEST},
    {"PCR_LOC_FORBIDDEN_WOODS_CLIMB_TO_TOP_USING_BOKO_BABA_BULBS", PCR_CLASS_CHEST},
    {"PCR_LOC_FORBIDDEN_WOODS_POT_HIGH_ABOVE_HOLLOW_TREE", PCR_CLASS_ACTR},
    {"PCR_LOC_FORBIDDEN_WOODS_HOLE_IN_TREE", PCR_CLASS_CHEST},
    {"PCR_LOC_FORBIDDEN_WOODS_MORTH_PIT", PCR_CLASS_CHEST},
    {"PCR_LOC_FORBIDDEN_WOODS_VINE_MAZE_LEFT_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_FORBIDDEN_WOODS_VINE_MAZE_RIGHT_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_FORBIDDEN_WOODS_HIGHEST_POT_IN_VINE_MAZE", PCR_CLASS_ACTR},
    {"PCR_LOC_FORBIDDEN_WOODS_TALL_ROOM_BEFORE_MINIBOSS", PCR_CLASS_CHEST},
    {"PCR_LOC_FORBIDDEN_WOODS_MOTHULA_MINIBOSS_ROOM", PCR_CLASS_CHEST},
    {"PCR_LOC_FORBIDDEN_WOODS_PAST_SEEDS_HANGING_BY_VINES", PCR_CLASS_CHEST},
    {"PCR_LOC_FORBIDDEN_WOODS_CHEST_ACROSS_RED_HANGING_FLOWER", PCR_CLASS_CHEST},
    {"PCR_LOC_FORBIDDEN_WOODS_TINGLE_STATUE_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_FORBIDDEN_WOODS_CHEST_IN_LOCKED_TREE_TRUNK", PCR_CLASS_CHEST},
    {"PCR_LOC_FORBIDDEN_WOODS_BIG_KEY_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_FORBIDDEN_WOODS_DOUBLE_MOTHULA_ROOM", PCR_CLASS_CHEST},
    {"PCR_LOC_FORBIDDEN_WOODS_KALLE_DEMOS_HEART_CONTAINER", PCR_CLASS_ACTR | PCR_CLASS_CODE},
    {"PCR_LOC_GREATFISH_ISLE_HIDDEN_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_TOWER_OF_THE_GODS_CHEST_BEHIND_BOMBABLE_WALLS", PCR_CLASS_CHEST},
    {"PCR_LOC_TOWER_OF_THE_GODS_POT_BEHIND_BOMBABLE_WALLS", PCR_CLASS_ACTR},
    {"PCR_LOC_TOWER_OF_THE_GODS_HOP_ACROSS_FLOATING_BOXES", PCR_CLASS_CHEST},
    {"PCR_LOC_TOWER_OF_THE_GODS_LIGHT_TWO_TORCHES", PCR_CLASS_CHEST},
    {"PCR_LOC_TOWER_OF_THE_GODS_SKULLS_ROOM_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_TOWER_OF_THE_GODS_SHOOT_EYE_ABOVE_SKULLS_ROOM_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_TOWER_OF_THE_GODS_TINGLE_STATUE_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_TOWER_OF_THE_GODS_FIRST_CHEST_GUARDED_BY_ARMOS_KNIGHTS", PCR_CLASS_CHEST},
    {"PCR_LOC_TOWER_OF_THE_GODS_STONE_TABLET", PCR_CLASS_EVENT},
    {"PCR_LOC_TOWER_OF_THE_GODS_DARKNUT_MINIBOSS_ROOM", PCR_CLASS_CHEST},
    {"PCR_LOC_TOWER_OF_THE_GODS_SECOND_CHEST_GUARDED_BY_ARMOS_KNIGHTS", PCR_CLASS_CHEST},
    {"PCR_LOC_TOWER_OF_THE_GODS_FLOATING_PLATFORMS_ROOM", PCR_CLASS_CHEST},
    {"PCR_LOC_TOWER_OF_THE_GODS_TOP_OF_FLOATING_PLATFORMS_ROOM", PCR_CLASS_CHEST},
    {"PCR_LOC_TOWER_OF_THE_GODS_EASTERN_POT_IN_BIG_KEY_CHEST_ROOM", PCR_CLASS_ACTR},
    {"PCR_LOC_TOWER_OF_THE_GODS_BIG_KEY_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_TOWER_OF_THE_GODS_GOHDAN_HEART_CONTAINER", PCR_CLASS_ACTR | PCR_CLASS_CODE},
    {"PCR_LOC_HYRULE_MASTER_SWORD_CHAMBER", PCR_CLASS_ADDED | PCR_CLASS_CODE},
    {"PCR_LOC_FORSAKEN_FORTRESS_PHANTOM_GANON", PCR_CLASS_CHEST},
    {"PCR_LOC_FORSAKEN_FORTRESS_CHEST_OUTSIDE_UPPER_JAIL_CELL", PCR_CLASS_CHEST},
    {"PCR_LOC_FORSAKEN_FORTRESS_CHEST_INSIDE_LOWER_JAIL_CELL", PCR_CLASS_CHEST},
    {"PCR_LOC_FORSAKEN_FORTRESS_CHEST_GUARDED_BY_BOKOBLIN", PCR_CLASS_CHEST},
    {"PCR_LOC_FORSAKEN_FORTRESS_CHEST_ON_BED", PCR_CLASS_CHEST},
    {"PCR_LOC_FORSAKEN_FORTRESS_HELMAROC_KING_HEART_CONTAINER", PCR_CLASS_ACTR | PCR_CLASS_CODE},
    {"PCR_LOC_MOTHER_AND_CHILD_ISLES_INSIDE_MOTHER_ISLE", PCR_CLASS_ADDED | PCR_CLASS_CODE},
    {"PCR_LOC_FIRE_MOUNTAIN_CAVE_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_FIRE_MOUNTAIN_LOOKOUT_PLATFORM_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_FIRE_MOUNTAIN_LOOKOUT_PLATFORM_DESTROY_THE_CANNONS", PCR_CLASS_CHEST},
    {"PCR_LOC_FIRE_MOUNTAIN_BIG_OCTO", PCR_CLASS_SCOB},
    {"PCR_LOC_ICE_RING_ISLE_FROZEN_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_ICE_RING_ISLE_CAVE_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_ICE_RING_ISLE_INNER_CAVE_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_HEADSTONE_ISLAND_TOP_OF_THE_ISLAND", PCR_CLASS_ACTR},
    {"PCR_LOC_HEADSTONE_ISLAND_SUBMARINE", PCR_CLASS_CHEST},
    {"PCR_LOC_EARTH_TEMPLE_TRANSPARENT_CHEST_IN_WARP_POT_ROOM", PCR_CLASS_CHEST},
    {"PCR_LOC_EARTH_TEMPLE_BEHIND_CURTAIN_IN_WARP_POT_ROOM", PCR_CLASS_ACTR},
    {"PCR_LOC_EARTH_TEMPLE_TRANSPARENT_CHEST_IN_FIRST_CRYPT", PCR_CLASS_CHEST},
    {"PCR_LOC_EARTH_TEMPLE_CHEST_BEHIND_DESTRUCTIBLE_WALLS", PCR_CLASS_CHEST},
    {"PCR_LOC_EARTH_TEMPLE_CHEST_IN_THREE_BLOCKS_ROOM", PCR_CLASS_CHEST},
    {"PCR_LOC_EARTH_TEMPLE_CHEST_BEHIND_STATUES", PCR_CLASS_CHEST},
    {"PCR_LOC_EARTH_TEMPLE_CASKET_IN_SECOND_CRYPT", PCR_CLASS_ACTR},
    {"PCR_LOC_EARTH_TEMPLE_STALFOS_MINIBOSS_ROOM", PCR_CLASS_CHEST},
    {"PCR_LOC_EARTH_TEMPLE_TINGLE_STATUE_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_EARTH_TEMPLE_END_OF_FOGGY_ROOM_WITH_FLOORMASTERS", PCR_CLASS_CHEST},
    {"PCR_LOC_EARTH_TEMPLE_KILL_ALL_FLOORMASTERS_IN_FOGGY_ROOM", PCR_CLASS_CHEST},
    {"PCR_LOC_EARTH_TEMPLE_BEHIND_CURTAIN_NEXT_TO_HAMMER_BUTTON", PCR_CLASS_ACTR},
    {"PCR_LOC_EARTH_TEMPLE_CHEST_IN_THIRD_CRYPT", PCR_CLASS_CHEST},
    {"PCR_LOC_EARTH_TEMPLE_MANY_MIRRORS_ROOM_RIGHT_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_EARTH_TEMPLE_MANY_MIRRORS_ROOM_LEFT_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_EARTH_TEMPLE_STALFOS_CRYPT_ROOM", PCR_CLASS_CHEST},
    {"PCR_LOC_EARTH_TEMPLE_BIG_KEY_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_EARTH_TEMPLE_JALHALLA_HEART_CONTAINER", PCR_CLASS_ACTR | PCR_CLASS_CODE},
    {"PCR_LOC_WIND_TEMPLE_CHEST_BETWEEN_TWO_DIRT_PATCHES", PCR_CLASS_CHEST},
    {"PCR_LOC_WIND_TEMPLE_BEHIND_STONE_HEAD_IN_HIDDEN_UPPER_ROOM", PCR_CLASS_ACTR},
    {"PCR_LOC_WIND_TEMPLE_TINGLE_STATUE_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_WIND_TEMPLE_CHEST_BEHIND_STONE_HEAD", PCR_CLASS_CHEST},
    {"PCR_LOC_WIND_TEMPLE_CHEST_IN_LEFT_ALCOVE", PCR_CLASS_CHEST},
    {"PCR_LOC_WIND_TEMPLE_BIG_KEY_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_WIND_TEMPLE_CHEST_IN_MANY_CYCLONES_ROOM", PCR_CLASS_CHEST},
    {"PCR_LOC_WIND_TEMPLE_BEHIND_STONE_HEAD_IN_MANY_CYCLONES_ROOM", PCR_CLASS_ACTR},
    {"PCR_LOC_WIND_TEMPLE_CHEST_IN_MIDDLE_OF_HUB_ROOM", PCR_CLASS_CHEST},
    {"PCR_LOC_WIND_TEMPLE_SPIKE_WALL_ROOM_FIRST_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_WIND_TEMPLE_SPIKE_WALL_ROOM_DESTROY_ALL_CRACKED_FLOORS", PCR_CLASS_CHEST},
    {"PCR_LOC_WIND_TEMPLE_WIZZROBE_MINIBOSS_ROOM", PCR_CLASS_CHEST},
    {"PCR_LOC_WIND_TEMPLE_CHEST_AT_TOP_OF_HUB_ROOM", PCR_CLASS_CHEST},
    {"PCR_LOC_WIND_TEMPLE_CHEST_BEHIND_SEVEN_ARMOS", PCR_CLASS_CHEST},
    {"PCR_LOC_WIND_TEMPLE_KILL_ALL_ENEMIES_IN_TALL_BASEMENT_ROOM", PCR_CLASS_CHEST},
    {"PCR_LOC_WIND_TEMPLE_MOLGERA_HEART_CONTAINER", PCR_CLASS_ACTR | PCR_CLASS_CODE},
    {"PCR_LOC_GANONS_TOWER_MAZE_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_MAILBOX_LETTER_FROM_HOSKITS_GIRLFRIEND", PCR_CLASS_CODE},
    {"PCR_LOC_MAILBOX_LETTER_FROM_BAITOS_MOTHER", PCR_CLASS_CODE},
    {"PCR_LOC_MAILBOX_LETTER_FROM_BAITO", PCR_CLASS_CODE},
    {"PCR_LOC_MAILBOX_LETTER_FROM_KOMALIS_FATHER", PCR_CLASS_CODE},
    {"PCR_LOC_MAILBOX_LETTER_ADVERTISING_BOMBS_IN_BEEDLES_SHOP", PCR_CLASS_CODE},
    {"PCR_LOC_MAILBOX_LETTER_ADVERTISING_ROCK_SPIRE_SHOP_SHIP", PCR_CLASS_CODE},
    {"PCR_LOC_MAILBOX_BEEDLES_SILVER_MEMBERSHIP_REWARD", PCR_CLASS_CODE},
    {"PCR_LOC_MAILBOX_BEEDLES_GOLD_MEMBERSHIP_REWARD", PCR_CLASS_CODE},
    {"PCR_LOC_MAILBOX_LETTER_FROM_ORCA", PCR_CLASS_CODE},
    {"PCR_LOC_MAILBOX_LETTER_FROM_GRANDMA", PCR_CLASS_CODE},
    {"PCR_LOC_MAILBOX_LETTER_FROM_ARYLL", PCR_CLASS_CODE},
    {"PCR_LOC_MAILBOX_LETTER_FROM_TINGLE", PCR_CLASS_CODE},
    {"PCR_LOC_THE_GREAT_SEA_BEEDLES_SHOP_SHIP_20_RUPEE_ITEM", PCR_CLASS_CODE},
    {"PCR_LOC_THE_GREAT_SEA_SALVAGE_CORP_GIFT", PCR_CLASS_CODE},
    {"PCR_LOC_THE_GREAT_SEA_CYCLOS", PCR_CLASS_EVENT},
    {"PCR_LOC_THE_GREAT_SEA_GORON_TRADING_REWARD", PCR_CLASS_CODE},
    {"PCR_LOC_THE_GREAT_SEA_WITHERED_TREES", PCR_CLASS_CODE},
    {"PCR_LOC_THE_GREAT_SEA_GHOST_SHIP", PCR_CLASS_CHEST},
    {"PCR_LOC_PRIVATE_OASIS_CHEST_AT_TOP_OF_WATERFALL", PCR_CLASS_CHEST},
    {"PCR_LOC_PRIVATE_OASIS_CABANA_LABYRINTH_LOWER_FLOOR_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_PRIVATE_OASIS_CABANA_LABYRINTH_UPPER_FLOOR_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_PRIVATE_OASIS_BIG_OCTO", PCR_CLASS_SCOB},
    {"PCR_LOC_SPECTACLE_ISLAND_BARREL_SHOOTING_FIRST_PRIZE", PCR_CLASS_CODE},
    {"PCR_LOC_SPECTACLE_ISLAND_BARREL_SHOOTING_SECOND_PRIZE", PCR_CLASS_CODE},
    {"PCR_LOC_NEEDLE_ROCK_ISLE_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_NEEDLE_ROCK_ISLE_CAVE", PCR_CLASS_CHEST},
    {"PCR_LOC_NEEDLE_ROCK_ISLE_GOLDEN_GUNBOAT", PCR_CLASS_SCOB},
    {"PCR_LOC_ANGULAR_ISLES_PEAK", PCR_CLASS_CHEST},
    {"PCR_LOC_ANGULAR_ISLES_CAVE", PCR_CLASS_CHEST},
    {"PCR_LOC_BOATING_COURSE_RAFT", PCR_CLASS_CHEST},
    {"PCR_LOC_BOATING_COURSE_CAVE", PCR_CLASS_CHEST},
    {"PCR_LOC_STONE_WATCHER_ISLAND_CAVE", PCR_CLASS_CHEST},
    {"PCR_LOC_STONE_WATCHER_ISLAND_LOOKOUT_PLATFORM_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_STONE_WATCHER_ISLAND_LOOKOUT_PLATFORM_DESTROY_THE_CANNONS", PCR_CLASS_CHEST},
    {"PCR_LOC_ISLET_OF_STEEL_INTERIOR", PCR_CLASS_CHEST},
    {"PCR_LOC_ISLET_OF_STEEL_LOOKOUT_PLATFORM_DEFEAT_THE_ENEMIES", PCR_CLASS_CHEST},
    {"PCR_LOC_OVERLOOK_ISLAND_CAVE", PCR_CLASS_CHEST},
    {"PCR_LOC_BIRDS_PEAK_ROCK_CAVE", PCR_CLASS_CHEST},
    {"PCR_LOC_PAWPRINT_ISLE_CHUCHU_CAVE_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_PAWPRINT_ISLE_CHUCHU_CAVE_BEHIND_LEFT_BOULDER", PCR_CLASS_CHEST},
    {"PCR_LOC_PAWPRINT_ISLE_CHUCHU_CAVE_BEHIND_RIGHT_BOULDER", PCR_CLASS_CHEST},
    {"PCR_LOC_PAWPRINT_ISLE_CHUCHU_CAVE_SCALE_THE_WALL", PCR_CLASS_CHEST},
    {"PCR_LOC_PAWPRINT_ISLE_WIZZROBE_CAVE", PCR_CLASS_CHEST},
    {"PCR_LOC_PAWPRINT_ISLE_LOOKOUT_PLATFORM_DEFEAT_THE_ENEMIES", PCR_CLASS_CHEST},
    {"PCR_LOC_THORNED_FAIRY_ISLAND_GREAT_FAIRY", PCR_CLASS_CODE},
    {"PCR_LOC_THORNED_FAIRY_ISLAND_NORTHEASTERN_LOOKOUT_PLATFORM_DESTROY_THE_CANNONS", PCR_CLASS_CHEST},
    {"PCR_LOC_THORNED_FAIRY_ISLAND_SOUTHWESTERN_LOOKOUT_PLATFORM_DEFEAT_THE_ENEMIES", PCR_CLASS_CHEST},
    {"PCR_LOC_EASTERN_FAIRY_ISLAND_GREAT_FAIRY", PCR_CLASS_CODE},
    {"PCR_LOC_EASTERN_FAIRY_ISLAND_LOOKOUT_PLATFORM_DEFEAT_THE_CANNONS_AND_ENEMIES", PCR_CLASS_CHEST},
    {"PCR_LOC_WESTERN_FAIRY_ISLAND_GREAT_FAIRY", PCR_CLASS_CODE},
    {"PCR_LOC_WESTERN_FAIRY_ISLAND_LOOKOUT_PLATFORM", PCR_CLASS_CHEST},
    {"PCR_LOC_SOUTHERN_FAIRY_ISLAND_GREAT_FAIRY", PCR_CLASS_CODE},
    {"PCR_LOC_SOUTHERN_FAIRY_ISLAND_LOOKOUT_PLATFORM_DESTROY_THE_NORTHWEST_CANNONS", PCR_CLASS_CHEST},
    {"PCR_LOC_SOUTHERN_FAIRY_ISLAND_LOOKOUT_PLATFORM_DESTROY_THE_SOUTHEAST_CANNONS", PCR_CLASS_CHEST},
    {"PCR_LOC_NORTHERN_FAIRY_ISLAND_GREAT_FAIRY", PCR_CLASS_CODE},
    {"PCR_LOC_NORTHERN_FAIRY_ISLAND_SUBMARINE", PCR_CLASS_CHEST},
    {"PCR_LOC_TINGLE_ISLAND_ANKLE_REWARD_FOR_ALL_TINGLE_STATUES", PCR_CLASS_CODE},
    {"PCR_LOC_TINGLE_ISLAND_BIG_OCTO", PCR_CLASS_SCOB},
    {"PCR_LOC_DIAMOND_STEPPE_ISLAND_WARP_MAZE_CAVE_FIRST_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_DIAMOND_STEPPE_ISLAND_WARP_MAZE_CAVE_SECOND_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_DIAMOND_STEPPE_ISLAND_BIG_OCTO", PCR_CLASS_SCOB},
    {"PCR_LOC_BOMB_ISLAND_CAVE", PCR_CLASS_CHEST},
    {"PCR_LOC_BOMB_ISLAND_LOOKOUT_PLATFORM_DEFEAT_THE_ENEMIES", PCR_CLASS_CHEST},
    {"PCR_LOC_BOMB_ISLAND_SUBMARINE", PCR_CLASS_CHEST},
    {"PCR_LOC_ROCK_SPIRE_ISLE_CAVE", PCR_CLASS_CHEST},
    {"PCR_LOC_ROCK_SPIRE_ISLE_BEEDLES_SPECIAL_SHOP_SHIP_500_RUPEE_ITEM", PCR_CLASS_CODE},
    {"PCR_LOC_ROCK_SPIRE_ISLE_BEEDLES_SPECIAL_SHOP_SHIP_950_RUPEE_ITEM", PCR_CLASS_CODE},
    {"PCR_LOC_ROCK_SPIRE_ISLE_BEEDLES_SPECIAL_SHOP_SHIP_900_RUPEE_ITEM", PCR_CLASS_CODE},
    {"PCR_LOC_ROCK_SPIRE_ISLE_WESTERN_LOOKOUT_PLATFORM_DESTROY_THE_CANNONS", PCR_CLASS_CHEST},
    {"PCR_LOC_ROCK_SPIRE_ISLE_EASTERN_LOOKOUT_PLATFORM_DESTROY_THE_CANNONS", PCR_CLASS_CHEST},
    {"PCR_LOC_ROCK_SPIRE_ISLE_CENTER_LOOKOUT_PLATFORM", PCR_CLASS_CHEST},
    {"PCR_LOC_ROCK_SPIRE_ISLE_SOUTHEAST_GUNBOAT", PCR_CLASS_SCOB},
    {"PCR_LOC_SHARK_ISLAND_CAVE", PCR_CLASS_CHEST},
    {"PCR_LOC_CLIFF_PLATEAU_ISLES_CAVE", PCR_CLASS_CHEST},
    {"PCR_LOC_CLIFF_PLATEAU_ISLES_HIGHEST_ISLE", PCR_CLASS_CHEST},
    {"PCR_LOC_CLIFF_PLATEAU_ISLES_LOOKOUT_PLATFORM", PCR_CLASS_CHEST},
    {"PCR_LOC_CRESCENT_MOON_ISLAND_CHEST", PCR_CLASS_CHEST},
    {"PCR_LOC_CRESCENT_MOON_ISLAND_SUBMARINE", PCR_CLASS_CHEST},
    {"PCR_LOC_HORSESHOE_ISLAND_PLAY_GOLF", PCR_CLASS_CHEST},
    {"PCR_LOC_HORSESHOE_ISLAND_CAVE", PCR_CLASS_CHEST},
    {"PCR_LOC_HORSESHOE_ISLAND_NORTHWESTERN_LOOKOUT_PLATFORM", PCR_CLASS_CHEST},
    {"PCR_LOC_HORSESHOE_ISLAND_SOUTHEASTERN_LOOKOUT_PLATFORM", PCR_CLASS_CHEST},
    {"PCR_LOC_FLIGHT_CONTROL_PLATFORM_BIRD_MAN_CONTEST_FIRST_PRIZE", PCR_CLASS_CODE},
    {"PCR_LOC_FLIGHT_CONTROL_PLATFORM_SUBMARINE", PCR_CLASS_CHEST},
    {"PCR_LOC_STAR_ISLAND_CAVE", PCR_CLASS_CHEST},
    {"PCR_LOC_STAR_ISLAND_LOOKOUT_PLATFORM", PCR_CLASS_CHEST},
    {"PCR_LOC_STAR_BELT_ARCHIPELAGO_LOOKOUT_PLATFORM", PCR_CLASS_CHEST},
    {"PCR_LOC_FIVE_STAR_ISLES_LOOKOUT_PLATFORM_DESTROY_THE_CANNONS", PCR_CLASS_CHEST},
    {"PCR_LOC_FIVE_STAR_ISLES_RAFT", PCR_CLASS_CHEST},
    {"PCR_LOC_FIVE_STAR_ISLES_SUBMARINE", PCR_CLASS_CHEST},
    {"PCR_LOC_SEVEN_STAR_ISLES_CENTER_LOOKOUT_PLATFORM", PCR_CLASS_CHEST},
    {"PCR_LOC_SEVEN_STAR_ISLES_NORTHERN_LOOKOUT_PLATFORM", PCR_CLASS_CHEST},
    {"PCR_LOC_SEVEN_STAR_ISLES_SOUTHERN_LOOKOUT_PLATFORM", PCR_CLASS_CHEST},
    {"PCR_LOC_SEVEN_STAR_ISLES_BIG_OCTO", PCR_CLASS_SCOB},
    {"PCR_LOC_CYCLOPS_REEF_DESTROY_THE_CANNONS_AND_GUNBOATS", PCR_CLASS_CHEST},
    {"PCR_LOC_CYCLOPS_REEF_LOOKOUT_PLATFORM_DEFEAT_THE_ENEMIES", PCR_CLASS_CHEST},
    {"PCR_LOC_TWO_EYE_REEF_DESTROY_THE_CANNONS_AND_GUNBOATS", PCR_CLASS_CHEST},
    {"PCR_LOC_TWO_EYE_REEF_LOOKOUT_PLATFORM", PCR_CLASS_CHEST},
    {"PCR_LOC_TWO_EYE_REEF_BIG_OCTO_GREAT_FAIRY", PCR_CLASS_EVENT},
    {"PCR_LOC_THREE_EYE_REEF_DESTROY_THE_CANNONS_AND_GUNBOATS", PCR_CLASS_CHEST},
    {"PCR_LOC_FOUR_EYE_REEF_DESTROY_THE_CANNONS_AND_GUNBOATS", PCR_CLASS_CHEST},
    {"PCR_LOC_FIVE_EYE_REEF_DESTROY_THE_CANNONS", PCR_CLASS_CHEST},
    {"PCR_LOC_FIVE_EYE_REEF_LOOKOUT_PLATFORM", PCR_CLASS_CHEST},
    {"PCR_LOC_SIX_EYE_REEF_DESTROY_THE_CANNONS_AND_GUNBOATS", PCR_CLASS_CHEST},
    {"PCR_LOC_SIX_EYE_REEF_LOOKOUT_PLATFORM_DESTROY_THE_CANNONS", PCR_CLASS_CHEST},
    {"PCR_LOC_SIX_EYE_REEF_SUBMARINE", PCR_CLASS_CHEST},
    {"PCR_LOC_FORSAKEN_FORTRESS_SECTOR_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_STAR_ISLAND_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_NORTHERN_FAIRY_ISLAND_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_GALE_ISLE_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_CRESCENT_MOON_ISLAND_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_SEVEN_STAR_ISLES_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_OVERLOOK_ISLAND_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_FOUR_EYE_REEF_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_MOTHER_AND_CHILD_ISLES_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_SPECTACLE_ISLAND_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_WINDFALL_ISLAND_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_PAWPRINT_ISLE_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_DRAGON_ROOST_ISLAND_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_FLIGHT_CONTROL_PLATFORM_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_WESTERN_FAIRY_ISLAND_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_ROCK_SPIRE_ISLE_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_TINGLE_ISLAND_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_NORTHERN_TRIANGLE_ISLAND_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_EASTERN_FAIRY_ISLAND_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_FIRE_MOUNTAIN_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_STAR_BELT_ARCHIPELAGO_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_THREE_EYE_REEF_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_GREATFISH_ISLE_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_CYCLOPS_REEF_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_SIX_EYE_REEF_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_TOWER_OF_THE_GODS_SECTOR_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_EASTERN_TRIANGLE_ISLAND_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_THORNED_FAIRY_ISLAND_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_NEEDLE_ROCK_ISLE_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_ISLET_OF_STEEL_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_STONE_WATCHER_ISLAND_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_SOUTHERN_TRIANGLE_ISLAND_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_PRIVATE_OASIS_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_BOMB_ISLAND_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_BIRDS_PEAK_ROCK_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_DIAMOND_STEPPE_ISLAND_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_FIVE_EYE_REEF_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_SHARK_ISLAND_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_SOUTHERN_FAIRY_ISLAND_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_ICE_RING_ISLE_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_FOREST_HAVEN_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_CLIFF_PLATEAU_ISLES_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_HORSESHOE_ISLAND_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_OUTSET_ISLAND_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_HEADSTONE_ISLAND_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_TWO_EYE_REEF_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_ANGULAR_ISLES_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_BOATING_COURSE_SUNKEN_TREASURE", PCR_CLASS_SCOB},
    {"PCR_LOC_FIVE_STAR_ISLES_SUNKEN_TREASURE", PCR_CLASS_SCOB},
};
