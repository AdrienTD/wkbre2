#include "tags.h" 
TagDict<72> Tags::GAMESET_tagDict({
    "3D_CLIP",
    "ACTION_SEQUENCE",
    "ANIMATION_TAG",
    "APPEARANCE_TAG",
    "ARMY",
    "ARMY_EXTENSION",
    "ASSOCIATE_CATEGORY",
    "BUILDING",
    "BUILDING_EXTENSION",
    "CAMERA_PATH",
    "CHARACTER",
    "CHARACTER_EXTENSION",
    "CHARACTER_LADDER",
    "CITY",
    "CITY_EXTENSION",
    "COMMAND",
    "COMMISSION",
    "CONDITION",
    "CONTAINER",
    "CONTAINER_EXTENSION",
    "DECLARE_ALIAS",
    "DECLARE_ASSOCIATE_CATEGORY",
    "DECLARE_ATTACHMENT_POINT_TYPE",
    "DECLARE_ITEM",
    "DEFAULT_DIPLOMATIC_STATUS",
    "DEFAULT_VALUE_TAG_INTERPRETATION",
    "DEFINE_VALUE",
    "DIPLOMATIC_LADDER",
    "EQUATION",
    "FOOTPRINT",
    "FORMATION",
    "FORMATION_EXTENSION",
    "GAME_EVENT",
    "GAME_TEXT_WINDOW",
    "GLOBAL_MAP_SOUND_TAG",
    "INDEV_ACTION_SEQUENCE",
    "INDEV_EQUATION",
    "INDEV_OBJECT_FINDER_DEFINITION",
    "INDIVIDUAL_ITEM",
    "LEVEL",
    "LEVEL_EXTENSION",
    "LINK_GAME_SET",
    "MARKER",
    "MARKER_EXTENSION",
    "MISSILE",
    "MISSILE_EXTENSION",
    "MUSIC_TAG",
    "OBJECT_CREATION",
    "OBJECT_FINDER_DEFINITION",
    "ORDER",
    "ORDER_ASSIGNMENT",
    "ORDER_CATEGORY",
    "PACKAGE",
    "PACKAGE_RECEIPT_TRIGGER",
    "PLAN",
    "PLAYER",
    "PLAYER_EXTENSION",
    "PROP",
    "PROP_EXTENSION",
    "REACTION",
    "SOUND",
    "SOUND_TAG",
    "TASK",
    "TASK_CATEGORY",
    "TERRAIN_ZONE",
    "TOWN",
    "TOWN_EXTENSION",
    "TYPE_TAG",
    "USER",
    "USER_EXTENSION",
    "VALUE_TAG",
    "WORK_ORDER",
});

TagDict<5> Tags::CCOMMAND_tagDict({
    "BLUEPRINT_TOOLTIP",
    "BUTTON_DEPRESSED",
    "BUTTON_ENABLED",
    "BUTTON_HIGHLIGHTED",
    "START_SEQUENCE",
});

TagDict<31> Tags::CBLUEPRINT_tagDict({
    "BLUEPRINT_TOOLTIP",
    "BUILDING_TYPE",
    "CAN_SPAWN",
    "DISPLAYS_ITEM",
    "DO_NOT_COLLIDE_WITH",
    "FLOATS_ON_WATER",
    "GENERATE_SIGHT_RANGE_EVENTS",
    "HAS_DYNAMIC_SHADOW",
    "HAS_STATIC_SHADOW",
    "INHERITS_FROM",
    "INTERPRET_VALUE_TAG_AS",
    "INTRINSIC_REACTION",
    "ITEM",
    "MAP_MUSIC_TAG",
    "MAP_SOUND_TAG",
    "MAP_SOUND_TAG_TO",
    "MAP_TYPE_TAG",
    "MISSILE_SPEED",
    "MOVEMENT_BAND",
    "MOVEMENT_SPEED_EQUATION",
    "OFFERS_COMMAND",
    "PHYSICAL_SUBTYPE",
    "RECEIVE_SIGHT_RANGE_EVENTS",
    "REPRESENT_AS",
    "SCALE_APPEARANCE",
    "SHOULD_PROCESS_SIGHT_RANGE",
    "SIGHT_RANGE_EQUATION",
    "STARTS_WITH_ITEM",
    "STRIKE_FLOOR_SOUND",
    "STRIKE_WATER_SOUND",
    "USE_FOOTPRINT",
});

TagDict<47> Tags::VALUE_tagDict({
    "AI_CONTROLLED",
    "ANGLE_BETWEEN",
    "ARE_ASSOCIATED",
    "BLUEPRINT_ITEM_VALUE",
    "BUILDING_TYPE",
    "BUILDING_TYPE_OPERAND",
    "CAN_AFFORD_COMMISSION",
    "CAN_REACH",
    "CAN_TRAVERSE_NEIGHBOURING_TILE",
    "CONSTANT",
    "COULD_REACH",
    "CURRENTLY_DOING_ORDER",
    "CURRENTLY_DOING_TASK",
    "DEFINED_VALUE",
    "DIPLOMATIC_STATUS_AT_LEAST",
    "DISTANCE_BETWEEN",
    "DISTANCE_BETWEEN_INCLUDING_RADIUS",
    "EQUATION_RESULT",
    "FINDER_RESULTS_COUNT",
    "GRADIENT_IN_FRONT",
    "HAS_APPEARANCE",
    "HAS_DIRECT_LINE_OF_SIGHT_TO",
    "INDEXED_ITEM_VALUE",
    "IS_ACCESSIBLE",
    "IS_DISABLED",
    "IS_DISCOVERED",
    "IS_IDLE",
    "IS_MUSIC_PLAYING",
    "IS_SUBSET_OF",
    "IS_VISIBLE",
    "ITEM_VALUE",
    "MAP_DEPTH",
    "MAP_WIDTH",
    "NUM_ASSOCIATES",
    "NUM_ASSOCIATORS",
    "NUM_OBJECTS",
    "NUM_ORDERS",
    "NUM_REFERENCERS",
    "OBJECT_CLASS",
    "OBJECT_ID",
    "OBJECT_TYPE",
    "SAME_PLAYER",
    "TILE_ITEM",
    "TOTAL_ITEM_VALUE",
    "VALUE_TAG_INTERPRETATION",
    "WATER_BENEATH",
    "WITHIN_FORWARD_ARC",
});

TagDict<27> Tags::ENODE_tagDict({
    "ABSOLUTE_VALUE",
    "ADDITION",
    "AND",
    "DIVISION",
    "EQUALS",
    "FRONT_BACK_LEFT_RIGHT",
    "GREATER_THAN",
    "GREATER_THAN_OR_EQUAL_TO",
    "IF_THEN_ELSE",
    "IS_BETWEEN",
    "IS_NEGATIVE",
    "IS_POSITIVE",
    "IS_ZERO",
    "LESS_THAN",
    "LESS_THAN_OR_EQUAL_TO",
    "MAX",
    "MIN",
    "MULTIPLICATION",
    "NEGATE",
    "NOT",
    "OR",
    "RANDOM_INTEGER",
    "RANDOM_RANGE",
    "RANDOM_UP_TO",
    "ROUND",
    "SUBTRACTION",
    "TRUNC",
});

TagDict<40> Tags::FINDER_tagDict({
    "AG_SELECTION",
    "ALIAS",
    "ALTERNATIVE",
    "ASSOCIATES",
    "ASSOCIATORS",
    "BEING_TRANSFERRED_TO_ME",
    "CANDIDATE",
    "CHAIN",
    "CHAIN_ORIGINAL_SELF",
    "COLLISION_SUBJECT",
    "CONTROLLER",
    "CREATOR",
    "DISABLED_ASSOCIATES",
    "DISCOVERED_UNITS",
    "FILTER",
    "FILTER_CANDIDATES",
    "FILTER_FIRST",
    "GRADE_SELECT",
    "GRADE_SELECT_CANDIDATES",
    "INTERSECTION",
    "LEVEL",
    "METRE_RADIUS",
    "NEAREST_CANDIDATE",
    "NEAREST_TO_SATISFY",
    "ORDER_GIVER",
    "PACKAGE_RELATED_PARTY",
    "PACKAGE_SENDER",
    "PLAYER",
    "PLAYERS",
    "RANDOM_SELECTION",
    "REFERENCERS",
    "RESULTS",
    "SELF",
    "SEQUENCE_EXECUTOR",
    "SPECIFIC_ID",
    "SUBORDINATES",
    "TARGET",
    "TILE_RADIUS",
    "UNION",
    "USER",
});

TagDict<17> Tags::POSITION_tagDict({
    "ABSOLUTE_POSITION",
    "AWAY_FROM",
    "CENTRE_OF_MAP",
    "DESTINATION_OF",
    "FIRING_ATTACHMENT_POINT",
    "IN_FRONT_OF",
    "LOCATION_OF",
    "MATCHING_OFFSET",
    "NEAREST_ATTACHMENT_POINT",
    "NEAREST_VALID_POSITION_FOR",
    "NEAREST_VALID_STAMPDOWN_POS",
    "OFFSET_FROM",
    "OUT_AT_ANGLE",
    "SPAWN_TILE_POSITION",
    "THE_OTHER_SIDE_OF",
    "THIS_SIDE_OF",
    "TOWARDS",
});

TagDict<118> Tags::ACTION_tagDict({
    "ACTIVATE_COMMISSION",
    "ADD_REACTION",
    "ADOPT_APPEARANCE_FOR",
    "ADOPT_DEFAULT_APPEARANCE_FOR",
    "ASSIGN_ALIAS",
    "ASSIGN_ORDER_VIA",
    "ATTACH_LOOPING_SPECIAL_EFFECT",
    "ATTACH_SPECIAL_EFFECT",
    "BOOT_LEVEL",
    "CANCEL_ORDER",
    "CHANGE_DIPLOMATIC_STATUS",
    "CHANGE_REACTION_PROFILE",
    "CLEAR_ALIAS",
    "CLEAR_ASSOCIATES",
    "COLLAPSING_CIRCLE_ON_MINIMAP",
    "CONQUER_LEVEL",
    "CONVERT_ACCORDING_TO_TAG",
    "CONVERT_TO",
    "COPY_FACING_OF",
    "CREATE_FORMATION",
    "CREATE_FORMATION_REFERENCE",
    "CREATE_OBJECT",
    "CREATE_OBJECT_VIA",
    "DEACTIVATE_COMMISSION",
    "DECLINE_DIPLOMATIC_OFFER",
    "DECREASE_INDEXED_ITEM",
    "DECREASE_ITEM",
    "DEREGISTER_ASSOCIATES",
    "DETACH_LOOPING_SPECIAL_EFFECT",
    "DISABLE",
    "DISABLE_DIPLOMATIC_REPORT_WINDOW",
    "DISABLE_GAME_INTERFACE",
    "DISABLE_TRIBUTES_WINDOW",
    "DISBAND_FORMATION",
    "DISPLAY_GAME_TEXT_WINDOW",
    "DISPLAY_LOAD_GAME_MENU",
    "ENABLE",
    "ENABLE_DIPLOMATIC_REPORT_WINDOW",
    "ENABLE_GAME_INTERFACE",
    "ENABLE_TRIBUTES_WINDOW",
    "EXECUTE_ONE_AT_RANDOM",
    "EXECUTE_SEQUENCE",
    "EXECUTE_SEQUENCE_AFTER_DELAY",
    "EXECUTE_SEQUENCE_OVER_PERIOD",
    "EXIT_TO_MAIN_MENU",
    "FACE_TOWARDS",
    "FADE_STOP_MUSIC",
    "FORCE_PLAY_MUSIC",
    "HIDE_CURRENT_GAME_TEXT_WINDOW",
    "HIDE_GAME_TEXT_WINDOW",
    "HIDE_MISSION_OBJECTIVES_ENTRY",
    "IDENTIFY_AND_MARK_CLUSTERS",
    "INCREASE_INDEXED_ITEM",
    "INCREASE_ITEM",
    "INTERPOLATE_CAMERA_TO_POSITION",
    "INTERPOLATE_CAMERA_TO_STORED_POSITION",
    "LEAVE_FORMATION",
    "LOCK_TIME",
    "MAKE_DIPLOMATIC_OFFER",
    "PLAY_ANIMATION_IF_IDLE",
    "PLAY_CAMERA_PATH",
    "PLAY_CLIP",
    "PLAY_MUSIC",
    "PLAY_SOUND",
    "PLAY_SOUND_AT_POSITION",
    "PLAY_SPECIAL_EFFECT",
    "PLAY_SPECIAL_EFFECT_BETWEEN",
    "REEVALUATE_TASK_TARGET",
    "REGISTER_ASSOCIATES",
    "REMOVE",
    "REMOVE_MULTIPLAYER_PLAYER",
    "REMOVE_REACTION",
    "REPEAT_SEQUENCE",
    "REPEAT_SEQUENCE_OVER_PERIOD",
    "REVEAL_FOG_OF_WAR",
    "SEND_CHAT_MESSAGE",
    "SEND_EVENT",
    "SEND_PACKAGE",
    "SET_ACTIVE_MISSION_OBJECTIVES",
    "SET_CHAT_PERSONALITY",
    "SET_INDEXED_ITEM",
    "SET_ITEM",
    "SET_RECONNAISSANCE",
    "SET_RENDERABLE",
    "SET_SCALE",
    "SET_SELECTABLE",
    "SET_TARGETABLE",
    "SHOW_BLINKING_DOT_ON_MINIMAP",
    "SHOW_MISSION_OBJECTIVES_ENTRY",
    "SHOW_MISSION_OBJECTIVES_ENTRY_INACTIVE",
    "SINK_AND_REMOVE",
    "SKIP_CAMERA_PATH_PLAYBACK",
    "SNAP_CAMERA_TO_POSITION",
    "SNAP_CAMERA_TO_STORED_POSITION",
    "STOP_CAMERA_PATH_PLAYBACK",
    "STOP_INDICATING_POSITION_OF_MISSION_OBJECTIVES_ENTRY",
    "STOP_SOUND",
    "STORE_CAMERA_POSITION",
    "SWITCH_APPEARANCE",
    "SWITCH_CONDITION",
    "SWITCH_HIGHEST",
    "SWITCH_ON_INTENSITY_MAP",
    "TELEPORT",
    "TERMINATE",
    "TERMINATE_ORDER",
    "TERMINATE_TASK",
    "TERMINATE_THIS_ORDER",
    "TERMINATE_THIS_TASK",
    "TRACE",
    "TRACE_FINDER_RESULTS",
    "TRACE_VALUE",
    "TRACK_OBJECT_POSITION_FROM_MISSION_OBJECTIVES_ENTRY",
    "TRANSFER_CONTROL",
    "UNASSIGN_ALIAS",
    "UNLOCK_LEVEL",
    "UNLOCK_TIME",
    "UPDATE_USER_PROFILE",
    "UPON_CONDITION",
});

TagDict<6> Tags::OBJCREATE_tagDict({
    "CONTROLLER",
    "CREATE_AT",
    "MAPPED_TYPE_TO_CREATE",
    "MATCH_APPEARANCE_OF",
    "POST_CREATION_SEQUENCE",
    "TYPE_TO_CREATE",
});

TagDict<15> Tags::SAVEGAME_tagDict({
    "CLIENT_STATE",
    "DELAYED_SEQUENCE_EXECUTION",
    "EXECUTE_SEQUENCE_OVER_PERIOD",
    "GAME_SET",
    "GAME_TYPE",
    "LEVEL",
    "NEXT_UNIQUE_ID",
    "NEXT_UPDATE_TIME_STAMP",
    "NUM_HUMAN_PLAYERS",
    "PART_OF_CAMPAIGN",
    "PREDEC",
    "REPEAT_SEQUENCE_OVER_PERIOD",
    "SERVER_NAME",
    "TIME_MANAGER_STATE",
    "UPDATE_ID",
});

TagDict<42> Tags::GAMEOBJ_tagDict({
    "AI_CONTROLLER",
    "ALIAS",
    "APPEARANCE",
    "ARMY",
    "ASSOCIATE",
    "BUILDING",
    "CHARACTER",
    "CITY",
    "COLOUR_INDEX",
    "CONTAINER",
    "DIPLOMATIC_OFFER",
    "DIPLOMATIC_STATUS_BETWEEN",
    "DISABLE_COUNT",
    "FOG_OF_WAR",
    "FORMATION",
    "INDIVIDUAL_REACTION",
    "ITEM",
    "MAP",
    "MARKER",
    "MISSILE",
    "NAME",
    "NEXT_UNIQUE_ID",
    "ORDER_CONFIGURATION",
    "ORIENTATION",
    "PARAM_BLOCK",
    "PLAYER",
    "PLAYER_TERMINATED",
    "POSITION",
    "PROP",
    "RECONNAISSANCE",
    "RECTANGLE",
    "RENDERABLE",
    "SCALE",
    "SELECTABLE",
    "START_CAMERA_ORIENTATION",
    "START_CAMERA_POS",
    "SUBTYPE",
    "TARGETABLE",
    "TERMINATED",
    "TERRAIN_ZONE",
    "TILES",
    "TOWN",
});

TagDict<14> Tags::GAMEOBJCLASS_tagDict({
    "ARMY",
    "BUILDING",
    "CHARACTER",
    "CITY",
    "CONTAINER",
    "FORMATION",
    "LEVEL",
    "MARKER",
    "MISSILE",
    "PLAYER",
    "PROP",
    "TERRAIN_ZONE",
    "TOWN",
    "USER",
});

TagDict<28> Tags::SETTING_tagDict({
    "ANIMATED_MODELS",
    "DATA_DIRECTORY",
    "ENABLE_GAMEPLAY_SHORTCUTS",
    "ENABLE_GAMESET_TRACING",
    "ENABLE_LANGUAGE_FILE",
    "FAR_Z_VALUE",
    "FOG",
    "FONT",
    "FULLSCREEN",
    "GAME_DIR",
    "GAME_SET_VERSION",
    "HARDWARE_CURSOR",
    "HARDWARE_VERTEX_PROCESSING",
    "IMGUI_FONT",
    "MAC_FILENAME_FALLBACK",
    "MAP_MAX_PART_SIZE",
    "MESH_BATCHING",
    "MULTI_BCP",
    "OCCLUSION_RATE",
    "PRELOAD_ALL_MODELS",
    "RENDERER",
    "R_OGL1_USE_BUFFER_OBJECTS",
    "SCREEN_SIZE",
    "SHOW_TIME_OBJ_INFO",
    "TEXTURE_COMPRESSION",
    "USE_MAP_TEXTURE_DATABASE",
    "VERTICAL_FOV",
    "VSYNC",
});

TagDict<17> Tags::STASK_tagDict({
    "COST_DEDUCTED",
    "DESTINATION",
    "FACE_TOWARDS",
    "FIRST_EXECUTION",
    "INITIAL_POSITION",
    "INITIAL_VELOCITY",
    "LAST_DESTINATION_VALID",
    "PROCESS_STATE",
    "PROXIMITY",
    "PROXIMITY_SATISFIED",
    "SPAWN_BLUEPRINT",
    "START_SEQUENCE_EXECUTED",
    "START_TIME",
    "TARGET",
    "TASK_ID",
    "TRIGGER",
    "TRIGGERS_STARTED",
});

TagDict<6> Tags::SORDER_tagDict({
    "CURRENT_TASK",
    "CYCLED",
    "ORDER_ID",
    "PROCESS_STATE",
    "TASK",
    "UNIQUE_TASK_ID",
});

TagDict<15> Tags::PDEVENT_tagDict({
    "ON_BUSY",
    "ON_CONTROL_TRANSFERRED",
    "ON_CONVERSION_END",
    "ON_DESTRUCTION",
    "ON_IDLE",
    "ON_LEVEL_START",
    "ON_OBJECT_ENTERS",
    "ON_OBJECT_EXITS",
    "ON_SEEING_OBJECT",
    "ON_SHARE_TILE",
    "ON_SPAWN",
    "ON_STAMPDOWN",
    "ON_STOP_SEEING_OBJECT",
    "ON_SUBORDINATE_RECEIVED",
    "ON_TERMINATION",
});

TagDict<10> Tags::ORDTSKTYPE_tagDict({
    "ATTACK",
    "BUILD",
    "FACE_TOWARDS",
    "FORMATION_CREATE",
    "MISSILE",
    "MOVE",
    "OBJECT_REFERENCE",
    "REPAIR",
    "SPAWN",
    "UPGRADE",
});

TagDict<10> Tags::CORDER_tagDict({
    "CANCELLATION_SEQUENCE",
    "CLASS_TYPE",
    "FLAG",
    "INITIALISATION_SEQUENCE",
    "IN_ORDER_CATEGORY",
    "RESUMPTION_SEQUENCE",
    "START_SEQUENCE",
    "SUSPENSION_SEQUENCE",
    "TERMINATION_SEQUENCE",
    "USE_TASK",
});

TagDict<23> Tags::CTASK_tagDict({
    "CANCELLATION_SEQUENCE",
    "CLASS_TYPE",
    "FLAG",
    "IDENTIFY_TARGET_EACH_CYCLE",
    "INITIALISATION_SEQUENCE",
    "IN_TASK_CATEGORY",
    "MOVEMENT_COMPLETED_SEQUENCE",
    "MOVEMENT_STARTED_SEQUENCE",
    "PLAY_ANIMATION",
    "PLAY_ANIMATION_ONCE",
    "PROXIMITY_DISSATISFIED_SEQUENCE",
    "PROXIMITY_REQUIREMENT",
    "PROXIMITY_SATISFIED_SEQUENCE",
    "REJECT_TARGET_IF_IT_IS_TERMINATED",
    "RESUMPTION_SEQUENCE",
    "START_SEQUENCE",
    "SUSPENSION_SEQUENCE",
    "SYNCH_ANIMATION_TO_FRACTION",
    "TASK_TARGET",
    "TERMINATE_ENTIRE_ORDER_IF_NO_TARGET",
    "TERMINATION_SEQUENCE",
    "TRIGGER",
    "USE_PREVIOUS_TASK_TARGET",
});

TagDict<3> Tags::ORDERASSIGNMODE_tagDict({
    "DO_FIRST",
    "DO_LAST",
    "FORGET_EVERYTHING_ELSE",
});

TagDict<3> Tags::CORDERASSIGN_tagDict({
    "ORDER_ASSIGNMENT_MODE",
    "ORDER_TARGET",
    "ORDER_TO_ASSIGN",
});

TagDict<6> Tags::TASKTRIGGER_tagDict({
    "ANIMATION_LOOP",
    "ATTACHMENT_POINT",
    "COLLISION",
    "STRUCK_FLOOR",
    "TIMER",
    "UNINTERRUPTIBLE_ANIMATION_LOOP",
});

TagDict<3> Tags::DISTCALCMODE_tagDict({
    "3D",
    "HORIZONTAL",
    "VERTICAL",
});

TagDict<9> Tags::OBJFINDCOND_tagDict({
    "ALLIED_UNITS",
    "BUILDINGS_ONLY",
    "CHARACTERS_AND_BUILDINGS_ONLY",
    "CHARACTERS_ONLY",
    "ENEMY_UNITS",
    "ORIGINAL_ALLIED_UNITS",
    "ORIGINAL_ENEMY_UNITS",
    "ORIGINAL_SAME_PLAYER_UNITS",
    "SAME_PLAYER_UNITS",
});

TagDict<3> Tags::PACKAGE_ITEM_MOD_tagDict({
    "INCREASE",
    "REDUCE",
    "REPLACE",
});

TagDict<4> Tags::GTW_BUTTON_WINDOW_ACTION_tagDict({
    "CLOSE_WINDOW",
    "MOVE_FIRST_PAGE",
    "MOVE_NEXT_PAGE",
    "MOVE_PREVIOUS_PAGE",
});

TagDict<4> Tags::SREQSTATE_tagDict({
    "ACTIVE",
    "BLOCKED",
    "COMPLETE",
    "STALLED",
});

TagDict<14> Tags::SREQDETAILEDSTATE_tagDict({
    "CANT_AFFORD",
    "FOUNDATION_CREATION_FAILED",
    "IMPOSSIBLE_CONDITION_FAILED",
    "NO_SPAWN_LOCATIONS_IDLE",
    "NO_SPAWN_LOCATION_FOUND",
    "NO_UPGRADE_LOCATIONS_IDLE",
    "NO_UPGRADE_LOCATION_FOUND",
    "NO_VALID_POSITION",
    "OK",
    "RESOURCE_REQUIRED_RESERVED",
    "SPAWN_ORDER_ASSIGNMENT_FAILED",
    "UNPROCESSED",
    "UPGRADE_ORDER_ASSIGNMENT_FAILED",
    "WAIT_CONDITION_FAILED",
});

TagDict<3> Tags::SREQUIREMENTCLASS_tagDict({
    "BUILDING_REQUIREMENT",
    "CHARACTER_REQUIREMENT",
    "UPGRADE_REQUIREMENT",
});

TagDict<12> Tags::BUILDINGTYPE_tagDict({
    "CIVIC",
    "CIVIC_CENTRE",
    "GATEHOUSE",
    "RURAL",
    "RURAL_CENTRE",
    "TOWER",
    "TOWER_CORNER_IN",
    "TOWER_CORNER_OUT",
    "WALL",
    "WALL_CORNER_IN",
    "WALL_CORNER_OUT",
    "WALL_CROSSROADS",
});

TagDict<9> Tags::PDITEM_tagDict({
    "ACTUAL_SIGHT_RANGE",
    "FOOD",
    "GOLD",
    "HP",
    "HPC",
    "HPC_OF_OBJ_BEING_SPAWNED",
    "HP_OF_OBJ_BEING_SPAWNED",
    "STONE",
    "WOOD",
});

TagDict<4> Tags::MAPTEXDIR_tagDict({
    "EAST",
    "NORTH",
    "SOUTH",
    "WEST",
});

TagDict<3> Tags::GS_SIDE_tagDict({
    "CRITICAL_FOR_CLIENT",
    "NORMAL",
    "SERVER_ONLY",
});

