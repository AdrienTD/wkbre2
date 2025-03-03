
#pragma once
#include "util/TagDict.h"

namespace Tags
{

const int GAMESET_3D_CLIP = 0;
const int GAMESET_ACTION_SEQUENCE = 1;
const int GAMESET_ANIMATION_TAG = 2;
const int GAMESET_APPEARANCE_TAG = 3;
const int GAMESET_ARMY = 4;
const int GAMESET_ARMY_CREATION_SCHEDULE = 5;
const int GAMESET_ARMY_EXTENSION = 6;
const int GAMESET_ASSOCIATE_CATEGORY = 7;
const int GAMESET_BUILDING = 8;
const int GAMESET_BUILDING_EXTENSION = 9;
const int GAMESET_CAMERA_PATH = 10;
const int GAMESET_CHARACTER = 11;
const int GAMESET_CHARACTER_EXTENSION = 12;
const int GAMESET_CHARACTER_LADDER = 13;
const int GAMESET_CITY = 14;
const int GAMESET_CITY_EXTENSION = 15;
const int GAMESET_COMMAND = 16;
const int GAMESET_COMMISSION = 17;
const int GAMESET_CONDITION = 18;
const int GAMESET_CONTAINER = 19;
const int GAMESET_CONTAINER_EXTENSION = 20;
const int GAMESET_DECLARE_ALIAS = 21;
const int GAMESET_DECLARE_ASSOCIATE_CATEGORY = 22;
const int GAMESET_DECLARE_ATTACHMENT_POINT_TYPE = 23;
const int GAMESET_DECLARE_ITEM = 24;
const int GAMESET_DEFAULT_DIPLOMATIC_STATUS = 25;
const int GAMESET_DEFAULT_VALUE_TAG_INTERPRETATION = 26;
const int GAMESET_DEFINE_VALUE = 27;
const int GAMESET_DIPLOMATIC_LADDER = 28;
const int GAMESET_EQUATION = 29;
const int GAMESET_FOOTPRINT = 30;
const int GAMESET_FORMATION = 31;
const int GAMESET_FORMATION_EXTENSION = 32;
const int GAMESET_GAME_EVENT = 33;
const int GAMESET_GAME_TEXT_WINDOW = 34;
const int GAMESET_GLOBAL_MAP_SOUND_TAG = 35;
const int GAMESET_GLOBAL_SPECIAL_EFFECT_MAPPING = 36;
const int GAMESET_INDEV_ACTION_SEQUENCE = 37;
const int GAMESET_INDEV_EQUATION = 38;
const int GAMESET_INDEV_OBJECT_FINDER_DEFINITION = 39;
const int GAMESET_INDIVIDUAL_ITEM = 40;
const int GAMESET_LEVEL = 41;
const int GAMESET_LEVEL_EXTENSION = 42;
const int GAMESET_LINK_GAME_SET = 43;
const int GAMESET_MARKER = 44;
const int GAMESET_MARKER_EXTENSION = 45;
const int GAMESET_MISSILE = 46;
const int GAMESET_MISSILE_EXTENSION = 47;
const int GAMESET_MUSIC_TAG = 48;
const int GAMESET_OBJECT_CREATION = 49;
const int GAMESET_OBJECT_FINDER_DEFINITION = 50;
const int GAMESET_ORDER = 51;
const int GAMESET_ORDER_ASSIGNMENT = 52;
const int GAMESET_ORDER_CATEGORY = 53;
const int GAMESET_PACKAGE = 54;
const int GAMESET_PACKAGE_RECEIPT_TRIGGER = 55;
const int GAMESET_PASSABILITY_FIELDS = 56;
const int GAMESET_PLAN = 57;
const int GAMESET_PLAYER = 58;
const int GAMESET_PLAYER_EXTENSION = 59;
const int GAMESET_PROP = 60;
const int GAMESET_PROP_EXTENSION = 61;
const int GAMESET_REACTION = 62;
const int GAMESET_SOUND = 63;
const int GAMESET_SOUND_TAG = 64;
const int GAMESET_SPECIAL_EFFECT_TAG = 65;
const int GAMESET_TASK = 66;
const int GAMESET_TASK_CATEGORY = 67;
const int GAMESET_TERRAIN = 68;
const int GAMESET_TERRAIN_BLUEPRINT_LIST = 69;
const int GAMESET_TERRAIN_ZONE = 70;
const int GAMESET_TOWN = 71;
const int GAMESET_TOWN_EXTENSION = 72;
const int GAMESET_TYPE_TAG = 73;
const int GAMESET_USER = 74;
const int GAMESET_USER_EXTENSION = 75;
const int GAMESET_VALUE_TAG = 76;
const int GAMESET_WORK_ORDER = 77;
const int GAMESET_COUNT = 78;
extern TagDict<78> GAMESET_tagDict;

const int CCOMMAND_BLUEPRINT_TOOLTIP = 0;
const int CCOMMAND_BUTTON_DEPRESSED = 1;
const int CCOMMAND_BUTTON_ENABLED = 2;
const int CCOMMAND_BUTTON_HIGHLIGHTED = 3;
const int CCOMMAND_START_SEQUENCE = 4;
const int CCOMMAND_COUNT = 5;
extern TagDict<5> CCOMMAND_tagDict;

const int CBLUEPRINT_BLUEPRINT_TOOLTIP = 0;
const int CBLUEPRINT_BUILDING_TYPE = 1;
const int CBLUEPRINT_CAN_BUILD = 2;
const int CBLUEPRINT_CAN_SPAWN = 3;
const int CBLUEPRINT_DISPLAYS_ITEM = 4;
const int CBLUEPRINT_DO_NOT_COLLIDE_WITH = 5;
const int CBLUEPRINT_FLOATS_ON_WATER = 6;
const int CBLUEPRINT_GENERATE_SIGHT_RANGE_EVENTS = 7;
const int CBLUEPRINT_HAS_DYNAMIC_SHADOW = 8;
const int CBLUEPRINT_HAS_STATIC_SHADOW = 9;
const int CBLUEPRINT_INHERITS_FROM = 10;
const int CBLUEPRINT_INTERPRET_VALUE_TAG_AS = 11;
const int CBLUEPRINT_INTRINSIC_REACTION = 12;
const int CBLUEPRINT_ITEM = 13;
const int CBLUEPRINT_MAP_MUSIC_TAG = 14;
const int CBLUEPRINT_MAP_SOUND_TAG = 15;
const int CBLUEPRINT_MAP_SOUND_TAG_TO = 16;
const int CBLUEPRINT_MAP_SPECIAL_EFFECT_TAG = 17;
const int CBLUEPRINT_MAP_TYPE_TAG = 18;
const int CBLUEPRINT_MISSILE_SPEED = 19;
const int CBLUEPRINT_MOVEMENT_BAND = 20;
const int CBLUEPRINT_MOVEMENT_SPEED_EQUATION = 21;
const int CBLUEPRINT_OFFERS_COMMAND = 22;
const int CBLUEPRINT_PHYSICAL_SUBTYPE = 23;
const int CBLUEPRINT_RECEIVE_SIGHT_RANGE_EVENTS = 24;
const int CBLUEPRINT_REMOVE_WHEN_NOT_REFERENCED = 25;
const int CBLUEPRINT_REPRESENT_AS = 26;
const int CBLUEPRINT_SCALE_APPEARANCE = 27;
const int CBLUEPRINT_SHOULD_PROCESS_SIGHT_RANGE = 28;
const int CBLUEPRINT_SIGHT_RANGE_EQUATION = 29;
const int CBLUEPRINT_STARTS_WITH_ITEM = 30;
const int CBLUEPRINT_STRIKE_FLOOR_SOUND = 31;
const int CBLUEPRINT_STRIKE_WATER_SOUND = 32;
const int CBLUEPRINT_USE_FOOTPRINT = 33;
const int CBLUEPRINT_VARY_X_SCALE = 34;
const int CBLUEPRINT_VARY_Y_SCALE = 35;
const int CBLUEPRINT_VARY_Z_SCALE = 36;
const int CBLUEPRINT_COUNT = 37;
extern TagDict<37> CBLUEPRINT_tagDict;

const int VALUE_AI_CONTROLLED = 0;
const int VALUE_ANGLE_BETWEEN = 1;
const int VALUE_ARE_ASSOCIATED = 2;
const int VALUE_AVERAGE_EQUATION_RESULT = 3;
const int VALUE_AVERAGE_ITEM_VALUE = 4;
const int VALUE_BLUEPRINT_ITEM_VALUE = 5;
const int VALUE_BUILDING_TYPE = 6;
const int VALUE_BUILDING_TYPE_OPERAND = 7;
const int VALUE_CAN_AFFORD_COMMISSION = 8;
const int VALUE_CAN_REACH = 9;
const int VALUE_CAN_TRAVERSE_NEIGHBOURING_TILE = 10;
const int VALUE_CONSTANT = 11;
const int VALUE_COULD_REACH = 12;
const int VALUE_CURRENTLY_DOING_ORDER = 13;
const int VALUE_CURRENTLY_DOING_TASK = 14;
const int VALUE_DEFINED_VALUE = 15;
const int VALUE_DIPLOMATIC_STATUS_AT_LEAST = 16;
const int VALUE_DISTANCE_BETWEEN = 17;
const int VALUE_DISTANCE_BETWEEN_INCLUDING_RADIUS = 18;
const int VALUE_EQUATION_RESULT = 19;
const int VALUE_FINDER_RESULTS_COUNT = 20;
const int VALUE_GRADIENT_IN_FRONT = 21;
const int VALUE_HAS_APPEARANCE = 22;
const int VALUE_HAS_DIRECT_LINE_OF_SIGHT_TO = 23;
const int VALUE_INDEXED_ITEM_VALUE = 24;
const int VALUE_IS_ACCESSIBLE = 25;
const int VALUE_IS_DISABLED = 26;
const int VALUE_IS_DISCOVERED = 27;
const int VALUE_IS_IDLE = 28;
const int VALUE_IS_IN_FRONT_OF = 29;
const int VALUE_IS_MUSIC_PLAYING = 30;
const int VALUE_IS_SUBSET_OF = 31;
const int VALUE_IS_VISIBLE = 32;
const int VALUE_ITEM_VALUE = 33;
const int VALUE_MAP_DEPTH = 34;
const int VALUE_MAP_WIDTH = 35;
const int VALUE_NUM_ASSOCIATES = 36;
const int VALUE_NUM_ASSOCIATORS = 37;
const int VALUE_NUM_OBJECTS = 38;
const int VALUE_NUM_ORDERS = 39;
const int VALUE_NUM_REFERENCERS = 40;
const int VALUE_OBJECT_CLASS = 41;
const int VALUE_OBJECT_ID = 42;
const int VALUE_OBJECT_TYPE = 43;
const int VALUE_SAME_PLAYER = 44;
const int VALUE_TILE_ITEM = 45;
const int VALUE_TOTAL_ITEM_VALUE = 46;
const int VALUE_VALUE_TAG_INTERPRETATION = 47;
const int VALUE_WATER_BENEATH = 48;
const int VALUE_WITHIN_FORWARD_ARC = 49;
const int VALUE_COUNT = 50;
extern TagDict<50> VALUE_tagDict;

const int ENODE_ABSOLUTE_VALUE = 0;
const int ENODE_ADDITION = 1;
const int ENODE_AND = 2;
const int ENODE_DIVISION = 3;
const int ENODE_EQUALS = 4;
const int ENODE_FRONT_BACK_LEFT_RIGHT = 5;
const int ENODE_GREATER_THAN = 6;
const int ENODE_GREATER_THAN_OR_EQUAL_TO = 7;
const int ENODE_IF_THEN_ELSE = 8;
const int ENODE_IS_BETWEEN = 9;
const int ENODE_IS_NEGATIVE = 10;
const int ENODE_IS_POSITIVE = 11;
const int ENODE_IS_ZERO = 12;
const int ENODE_LESS_THAN = 13;
const int ENODE_LESS_THAN_OR_EQUAL_TO = 14;
const int ENODE_MAX = 15;
const int ENODE_MIN = 16;
const int ENODE_MULTIPLICATION = 17;
const int ENODE_NEGATE = 18;
const int ENODE_NOT = 19;
const int ENODE_OR = 20;
const int ENODE_RANDOM_INTEGER = 21;
const int ENODE_RANDOM_RANGE = 22;
const int ENODE_RANDOM_UP_TO = 23;
const int ENODE_ROUND = 24;
const int ENODE_SUBTRACTION = 25;
const int ENODE_TRUNC = 26;
const int ENODE_COUNT = 27;
extern TagDict<27> ENODE_tagDict;

const int FINDER_AG_SELECTION = 0;
const int FINDER_ALIAS = 1;
const int FINDER_ALTERNATIVE = 2;
const int FINDER_ASSOCIATES = 3;
const int FINDER_ASSOCIATORS = 4;
const int FINDER_BEING_TRANSFERRED_TO_ME = 5;
const int FINDER_CANDIDATE = 6;
const int FINDER_CHAIN = 7;
const int FINDER_CHAIN_ORIGINAL_SELF = 8;
const int FINDER_COLLISION_SUBJECT = 9;
const int FINDER_CONTROLLER = 10;
const int FINDER_CREATOR = 11;
const int FINDER_DISABLED_ASSOCIATES = 12;
const int FINDER_DISCOVERED_UNITS = 13;
const int FINDER_FILTER = 14;
const int FINDER_FILTER_CANDIDATES = 15;
const int FINDER_FILTER_FIRST = 16;
const int FINDER_GRADE_SELECT = 17;
const int FINDER_GRADE_SELECT_CANDIDATES = 18;
const int FINDER_INTERSECTION = 19;
const int FINDER_LEVEL = 20;
const int FINDER_METRE_RADIUS = 21;
const int FINDER_NEAREST_CANDIDATE = 22;
const int FINDER_NEAREST_TO_SATISFY = 23;
const int FINDER_ORDER_GIVER = 24;
const int FINDER_PACKAGE_RELATED_PARTY = 25;
const int FINDER_PACKAGE_SENDER = 26;
const int FINDER_PLAYER = 27;
const int FINDER_PLAYERS = 28;
const int FINDER_RANDOM_SELECTION = 29;
const int FINDER_REFERENCERS = 30;
const int FINDER_RESULTS = 31;
const int FINDER_SELECTED_OBJECT = 32;
const int FINDER_SELF = 33;
const int FINDER_SEQUENCE_EXECUTOR = 34;
const int FINDER_SPECIFIC_ID = 35;
const int FINDER_SUBORDINATES = 36;
const int FINDER_TARGET = 37;
const int FINDER_TILE_RADIUS = 38;
const int FINDER_UNION = 39;
const int FINDER_USER = 40;
const int FINDER_COUNT = 41;
extern TagDict<41> FINDER_tagDict;

const int POSITION_ABSOLUTE_POSITION = 0;
const int POSITION_AWAY_FROM = 1;
const int POSITION_CENTRE_OF_MAP = 2;
const int POSITION_DESTINATION_OF = 3;
const int POSITION_FIRING_ATTACHMENT_POINT = 4;
const int POSITION_IN_FRONT_OF = 5;
const int POSITION_LOCATION_OF = 6;
const int POSITION_MATCHING_OFFSET = 7;
const int POSITION_NEAREST_ATTACHMENT_POINT = 8;
const int POSITION_NEAREST_CONSTRUCTION_SITE = 9;
const int POSITION_NEAREST_VALID_POSITION_FOR = 10;
const int POSITION_NEAREST_VALID_STAMPDOWN_POS = 11;
const int POSITION_OFFSET_FROM = 12;
const int POSITION_OUT_AT_ANGLE = 13;
const int POSITION_RANDOM_ATTACHMENT_POINT = 14;
const int POSITION_SPAWN_TILE_POSITION = 15;
const int POSITION_THE_OTHER_SIDE_OF = 16;
const int POSITION_THIS_SIDE_OF = 17;
const int POSITION_TOWARDS = 18;
const int POSITION_COUNT = 19;
extern TagDict<19> POSITION_tagDict;

const int ACTION_ABANDON_PLAN = 0;
const int ACTION_ACTIVATE_COMMISSION = 1;
const int ACTION_ACTIVATE_PLAN = 2;
const int ACTION_ADD_REACTION = 3;
const int ACTION_ADOPT_APPEARANCE_FOR = 4;
const int ACTION_ADOPT_DEFAULT_APPEARANCE_FOR = 5;
const int ACTION_ASSIGN_ALIAS = 6;
const int ACTION_ASSIGN_ORDER_VIA = 7;
const int ACTION_ATTACH_LOOPING_SPECIAL_EFFECT = 8;
const int ACTION_ATTACH_SPECIAL_EFFECT = 9;
const int ACTION_BOOT_LEVEL = 10;
const int ACTION_CANCEL_ORDER = 11;
const int ACTION_CHANGE_DIPLOMATIC_STATUS = 12;
const int ACTION_CHANGE_REACTION_PROFILE = 13;
const int ACTION_CLEAR_ALIAS = 14;
const int ACTION_CLEAR_ASSOCIATES = 15;
const int ACTION_COLLAPSING_CIRCLE_ON_MINIMAP = 16;
const int ACTION_CONQUER_LEVEL = 17;
const int ACTION_CONVERT_ACCORDING_TO_TAG = 18;
const int ACTION_CONVERT_TO = 19;
const int ACTION_COPY_FACING_OF = 20;
const int ACTION_CREATE_FORMATION = 21;
const int ACTION_CREATE_FORMATION_REFERENCE = 22;
const int ACTION_CREATE_OBJECT = 23;
const int ACTION_CREATE_OBJECT_VIA = 24;
const int ACTION_DEACTIVATE_COMMISSION = 25;
const int ACTION_DECLINE_DIPLOMATIC_OFFER = 26;
const int ACTION_DECREASE_INDEXED_ITEM = 27;
const int ACTION_DECREASE_ITEM = 28;
const int ACTION_DEREGISTER_ASSOCIATES = 29;
const int ACTION_DETACH_LOOPING_SPECIAL_EFFECT = 30;
const int ACTION_DISABLE = 31;
const int ACTION_DISABLE_DIPLOMACY_WINDOW = 32;
const int ACTION_DISABLE_DIPLOMATIC_REPORT_WINDOW = 33;
const int ACTION_DISABLE_GAME_INTERFACE = 34;
const int ACTION_DISABLE_TRIBUTES_WINDOW = 35;
const int ACTION_DISBAND_FORMATION = 36;
const int ACTION_DISPLAY_GAME_TEXT_WINDOW = 37;
const int ACTION_DISPLAY_LOAD_GAME_MENU = 38;
const int ACTION_ENABLE = 39;
const int ACTION_ENABLE_DIPLOMACY_WINDOW = 40;
const int ACTION_ENABLE_DIPLOMATIC_REPORT_WINDOW = 41;
const int ACTION_ENABLE_GAME_INTERFACE = 42;
const int ACTION_ENABLE_TRIBUTES_WINDOW = 43;
const int ACTION_EXECUTE_ONE_AT_RANDOM = 44;
const int ACTION_EXECUTE_SEQUENCE = 45;
const int ACTION_EXECUTE_SEQUENCE_AFTER_DELAY = 46;
const int ACTION_EXECUTE_SEQUENCE_OVER_PERIOD = 47;
const int ACTION_EXIT_TO_MAIN_MENU = 48;
const int ACTION_FACE_TOWARDS = 49;
const int ACTION_FADE_STOP_MUSIC = 50;
const int ACTION_FORCE_PLAY_MUSIC = 51;
const int ACTION_HIDE_CURRENT_GAME_TEXT_WINDOW = 52;
const int ACTION_HIDE_GAME_TEXT_WINDOW = 53;
const int ACTION_HIDE_MISSION_OBJECTIVES_ENTRY = 54;
const int ACTION_IDENTIFY_AND_MARK_CLUSTERS = 55;
const int ACTION_INCREASE_INDEXED_ITEM = 56;
const int ACTION_INCREASE_ITEM = 57;
const int ACTION_INDICATE_POSITION_OF_MISSION_OBJECTIVES_ENTRY = 58;
const int ACTION_INTERPOLATE_CAMERA_TO_POSITION = 59;
const int ACTION_INTERPOLATE_CAMERA_TO_STORED_POSITION = 60;
const int ACTION_LEAVE_FORMATION = 61;
const int ACTION_LOCK_TIME = 62;
const int ACTION_MAKE_DIPLOMATIC_OFFER = 63;
const int ACTION_PLAY_ANIMATION_IF_IDLE = 64;
const int ACTION_PLAY_CAMERA_PATH = 65;
const int ACTION_PLAY_CLIP = 66;
const int ACTION_PLAY_MUSIC = 67;
const int ACTION_PLAY_SOUND = 68;
const int ACTION_PLAY_SOUND_AT_POSITION = 69;
const int ACTION_PLAY_SPECIAL_EFFECT = 70;
const int ACTION_PLAY_SPECIAL_EFFECT_BETWEEN = 71;
const int ACTION_REEVALUATE_TASK_TARGET = 72;
const int ACTION_REGISTER_ASSOCIATES = 73;
const int ACTION_REGISTER_VICTOR = 74;
const int ACTION_REGISTER_WORK_ORDER = 75;
const int ACTION_REMOVE = 76;
const int ACTION_REMOVE_MULTIPLAYER_PLAYER = 77;
const int ACTION_REMOVE_REACTION = 78;
const int ACTION_REPEAT_SEQUENCE = 79;
const int ACTION_REPEAT_SEQUENCE_OVER_PERIOD = 80;
const int ACTION_RESERVE_SPACE_FOR_CONSTRUCTION = 81;
const int ACTION_RESERVE_TILE_FOR_CONSTRUCTION = 82;
const int ACTION_REVEAL_FOG_OF_WAR = 83;
const int ACTION_SEND_CHAT_MESSAGE = 84;
const int ACTION_SEND_EVENT = 85;
const int ACTION_SEND_PACKAGE = 86;
const int ACTION_SET_ACTIVE_MISSION_OBJECTIVES = 87;
const int ACTION_SET_CHAT_PERSONALITY = 88;
const int ACTION_SET_INDEXED_ITEM = 89;
const int ACTION_SET_ITEM = 90;
const int ACTION_SET_RECONNAISSANCE = 91;
const int ACTION_SET_RENDERABLE = 92;
const int ACTION_SET_SCALE = 93;
const int ACTION_SET_SELECTABLE = 94;
const int ACTION_SET_TARGETABLE = 95;
const int ACTION_SHOW_BLINKING_DOT_ON_MINIMAP = 96;
const int ACTION_SHOW_MISSION_OBJECTIVES_ENTRY = 97;
const int ACTION_SHOW_MISSION_OBJECTIVES_ENTRY_INACTIVE = 98;
const int ACTION_SINK_AND_REMOVE = 99;
const int ACTION_SKIP_CAMERA_PATH_PLAYBACK = 100;
const int ACTION_SNAP_CAMERA_TO_POSITION = 101;
const int ACTION_SNAP_CAMERA_TO_STORED_POSITION = 102;
const int ACTION_STOP_CAMERA_PATH_PLAYBACK = 103;
const int ACTION_STOP_INDICATING_POSITION_OF_MISSION_OBJECTIVES_ENTRY = 104;
const int ACTION_STOP_SOUND = 105;
const int ACTION_STORE_CAMERA_POSITION = 106;
const int ACTION_SWITCH_APPEARANCE = 107;
const int ACTION_SWITCH_CONDITION = 108;
const int ACTION_SWITCH_HIGHEST = 109;
const int ACTION_SWITCH_ON_INTENSITY_MAP = 110;
const int ACTION_TELEPORT = 111;
const int ACTION_TERMINATE = 112;
const int ACTION_TERMINATE_ORDER = 113;
const int ACTION_TERMINATE_TASK = 114;
const int ACTION_TERMINATE_THIS_ORDER = 115;
const int ACTION_TERMINATE_THIS_TASK = 116;
const int ACTION_TRACE = 117;
const int ACTION_TRACE_FINDER_RESULTS = 118;
const int ACTION_TRACE_POSITION = 119;
const int ACTION_TRACE_POSITIONS_OF = 120;
const int ACTION_TRACE_VALUE = 121;
const int ACTION_TRACK_OBJECT_POSITION_FROM_MISSION_OBJECTIVES_ENTRY = 122;
const int ACTION_TRANSFER_CONTROL = 123;
const int ACTION_UNASSIGN_ALIAS = 124;
const int ACTION_UNLOCK_LEVEL = 125;
const int ACTION_UNLOCK_TIME = 126;
const int ACTION_UPDATE_USER_PROFILE = 127;
const int ACTION_UPON_CONDITION = 128;
const int ACTION_COUNT = 129;
extern TagDict<129> ACTION_tagDict;

const int OBJCREATE_CONTROLLER = 0;
const int OBJCREATE_CREATE_AT = 1;
const int OBJCREATE_MAPPED_TYPE_TO_CREATE = 2;
const int OBJCREATE_MATCH_APPEARANCE_OF = 3;
const int OBJCREATE_POST_CREATION_SEQUENCE = 4;
const int OBJCREATE_TYPE_TO_CREATE = 5;
const int OBJCREATE_COUNT = 6;
extern TagDict<6> OBJCREATE_tagDict;

const int SAVEGAME_CLIENT_STATE = 0;
const int SAVEGAME_DELAYED_SEQUENCE_EXECUTION = 1;
const int SAVEGAME_EXECUTE_SEQUENCE_OVER_PERIOD = 2;
const int SAVEGAME_GAME_SET = 3;
const int SAVEGAME_GAME_TYPE = 4;
const int SAVEGAME_LEVEL = 5;
const int SAVEGAME_NEXT_UNIQUE_ID = 6;
const int SAVEGAME_NEXT_UPDATE_TIME_STAMP = 7;
const int SAVEGAME_NUM_HUMAN_PLAYERS = 8;
const int SAVEGAME_PART_OF_CAMPAIGN = 9;
const int SAVEGAME_PREDEC = 10;
const int SAVEGAME_REPEAT_SEQUENCE_OVER_PERIOD = 11;
const int SAVEGAME_SERVER_NAME = 12;
const int SAVEGAME_TIME_MANAGER_STATE = 13;
const int SAVEGAME_UPDATE_ID = 14;
const int SAVEGAME_COUNT = 15;
extern TagDict<15> SAVEGAME_tagDict;

const int GAMEOBJ_AI_CONTROLLER = 0;
const int GAMEOBJ_ALIAS = 1;
const int GAMEOBJ_APPEARANCE = 2;
const int GAMEOBJ_ARMY = 3;
const int GAMEOBJ_ASSOCIATE = 4;
const int GAMEOBJ_BUILDING = 5;
const int GAMEOBJ_CHARACTER = 6;
const int GAMEOBJ_CITY = 7;
const int GAMEOBJ_COLOUR_INDEX = 8;
const int GAMEOBJ_CONTAINER = 9;
const int GAMEOBJ_DIPLOMATIC_OFFER = 10;
const int GAMEOBJ_DIPLOMATIC_STATUS_BETWEEN = 11;
const int GAMEOBJ_DISABLE_COUNT = 12;
const int GAMEOBJ_FOG_OF_WAR = 13;
const int GAMEOBJ_FORMATION = 14;
const int GAMEOBJ_INDIVIDUAL_REACTION = 15;
const int GAMEOBJ_ITEM = 16;
const int GAMEOBJ_MAP = 17;
const int GAMEOBJ_MARKER = 18;
const int GAMEOBJ_MISSILE = 19;
const int GAMEOBJ_NAME = 20;
const int GAMEOBJ_NEXT_UNIQUE_ID = 21;
const int GAMEOBJ_ORDER_CONFIGURATION = 22;
const int GAMEOBJ_ORIENTATION = 23;
const int GAMEOBJ_PARAM_BLOCK = 24;
const int GAMEOBJ_PLAYER = 25;
const int GAMEOBJ_PLAYER_TERMINATED = 26;
const int GAMEOBJ_POSITION = 27;
const int GAMEOBJ_PROP = 28;
const int GAMEOBJ_RECONNAISSANCE = 29;
const int GAMEOBJ_RECTANGLE = 30;
const int GAMEOBJ_RENDERABLE = 31;
const int GAMEOBJ_SCALE = 32;
const int GAMEOBJ_SELECTABLE = 33;
const int GAMEOBJ_START_CAMERA_ORIENTATION = 34;
const int GAMEOBJ_START_CAMERA_POS = 35;
const int GAMEOBJ_SUBTYPE = 36;
const int GAMEOBJ_TARGETABLE = 37;
const int GAMEOBJ_TERMINATED = 38;
const int GAMEOBJ_TERRAIN_ZONE = 39;
const int GAMEOBJ_TILES = 40;
const int GAMEOBJ_TOWN = 41;
const int GAMEOBJ_COUNT = 42;
extern TagDict<42> GAMEOBJ_tagDict;

const int GAMEOBJCLASS_ARMY = 0;
const int GAMEOBJCLASS_BUILDING = 1;
const int GAMEOBJCLASS_CHARACTER = 2;
const int GAMEOBJCLASS_CITY = 3;
const int GAMEOBJCLASS_CONTAINER = 4;
const int GAMEOBJCLASS_FORMATION = 5;
const int GAMEOBJCLASS_LEVEL = 6;
const int GAMEOBJCLASS_MARKER = 7;
const int GAMEOBJCLASS_MISSILE = 8;
const int GAMEOBJCLASS_PLAYER = 9;
const int GAMEOBJCLASS_PROP = 10;
const int GAMEOBJCLASS_TERRAIN_ZONE = 11;
const int GAMEOBJCLASS_TOWN = 12;
const int GAMEOBJCLASS_USER = 13;
const int GAMEOBJCLASS_COUNT = 14;
extern TagDict<14> GAMEOBJCLASS_tagDict;

const int SETTING_ANIMATED_MODELS = 0;
const int SETTING_DATA_DIRECTORY = 1;
const int SETTING_ENABLE_GAMEPLAY_SHORTCUTS = 2;
const int SETTING_ENABLE_GAMESET_TRACING = 3;
const int SETTING_ENABLE_LANGUAGE_FILE = 4;
const int SETTING_FAR_Z_VALUE = 5;
const int SETTING_FOG = 6;
const int SETTING_FONT = 7;
const int SETTING_FULLSCREEN = 8;
const int SETTING_GAME_DIR = 9;
const int SETTING_GAME_SET_VERSION = 10;
const int SETTING_HARDWARE_CURSOR = 11;
const int SETTING_HARDWARE_VERTEX_PROCESSING = 12;
const int SETTING_IMGUI_FONT = 13;
const int SETTING_MAC_FILENAME_FALLBACK = 14;
const int SETTING_MAP_MAX_PART_SIZE = 15;
const int SETTING_MESH_BATCHING = 16;
const int SETTING_MULTI_BCP = 17;
const int SETTING_OCCLUSION_RATE = 18;
const int SETTING_PRELOAD_ALL_MODELS = 19;
const int SETTING_RENDERER = 20;
const int SETTING_R_OGL1_USE_BUFFER_OBJECTS = 21;
const int SETTING_SCREEN_SIZE = 22;
const int SETTING_SHOW_TIME_OBJ_INFO = 23;
const int SETTING_TEXTURE_COMPRESSION = 24;
const int SETTING_USE_MAP_TEXTURE_DATABASE = 25;
const int SETTING_VERTICAL_FOV = 26;
const int SETTING_VSYNC = 27;
const int SETTING_COUNT = 28;
extern TagDict<28> SETTING_tagDict;

const int STASK_COST_DEDUCTED = 0;
const int STASK_DESTINATION = 1;
const int STASK_FACE_TOWARDS = 2;
const int STASK_FIRST_EXECUTION = 3;
const int STASK_INITIAL_POSITION = 4;
const int STASK_INITIAL_VELOCITY = 5;
const int STASK_LAST_DESTINATION_VALID = 6;
const int STASK_PROCESS_STATE = 7;
const int STASK_PROXIMITY = 8;
const int STASK_PROXIMITY_SATISFIED = 9;
const int STASK_SPAWN_BLUEPRINT = 10;
const int STASK_START_SEQUENCE_EXECUTED = 11;
const int STASK_START_TIME = 12;
const int STASK_TARGET = 13;
const int STASK_TASK_ID = 14;
const int STASK_TRIGGER = 15;
const int STASK_TRIGGERS_STARTED = 16;
const int STASK_COUNT = 17;
extern TagDict<17> STASK_tagDict;

const int SORDER_CURRENT_TASK = 0;
const int SORDER_CYCLED = 1;
const int SORDER_ORDER_ID = 2;
const int SORDER_PROCESS_STATE = 3;
const int SORDER_TASK = 4;
const int SORDER_UNIQUE_TASK_ID = 5;
const int SORDER_COUNT = 6;
extern TagDict<6> SORDER_tagDict;

const int PDEVENT_ON_BUSY = 0;
const int PDEVENT_ON_COMMISSIONED = 1;
const int PDEVENT_ON_CONTROL_TRANSFERRED = 2;
const int PDEVENT_ON_CONVERSION_END = 3;
const int PDEVENT_ON_DESTRUCTION = 4;
const int PDEVENT_ON_FORMATION_DISBAND = 5;
const int PDEVENT_ON_IDLE = 6;
const int PDEVENT_ON_LAST_SUBORDINATE_RELEASED = 7;
const int PDEVENT_ON_LEVEL_START = 8;
const int PDEVENT_ON_OBJECT_ENTERS = 9;
const int PDEVENT_ON_OBJECT_EXITS = 10;
const int PDEVENT_ON_SEEING_OBJECT = 11;
const int PDEVENT_ON_SHARE_TILE = 12;
const int PDEVENT_ON_SPAWN = 13;
const int PDEVENT_ON_STAMPDOWN = 14;
const int PDEVENT_ON_STOP_SEEING_OBJECT = 15;
const int PDEVENT_ON_SUBORDINATE_RECEIVED = 16;
const int PDEVENT_ON_TERMINATION = 17;
const int PDEVENT_COUNT = 18;
extern TagDict<18> PDEVENT_tagDict;

const int ORDTSKTYPE_ATTACK = 0;
const int ORDTSKTYPE_BUILD = 1;
const int ORDTSKTYPE_FACE_TOWARDS = 2;
const int ORDTSKTYPE_FORMATION_CREATE = 3;
const int ORDTSKTYPE_MISSILE = 4;
const int ORDTSKTYPE_MOVE = 5;
const int ORDTSKTYPE_OBJECT_REFERENCE = 6;
const int ORDTSKTYPE_REPAIR = 7;
const int ORDTSKTYPE_SPAWN = 8;
const int ORDTSKTYPE_UPGRADE = 9;
const int ORDTSKTYPE_COUNT = 10;
extern TagDict<10> ORDTSKTYPE_tagDict;

const int CORDER_CANCELLATION_SEQUENCE = 0;
const int CORDER_CLASS_TYPE = 1;
const int CORDER_FLAG = 2;
const int CORDER_INITIALISATION_SEQUENCE = 3;
const int CORDER_IN_ORDER_CATEGORY = 4;
const int CORDER_RESUMPTION_SEQUENCE = 5;
const int CORDER_START_SEQUENCE = 6;
const int CORDER_SUSPENSION_SEQUENCE = 7;
const int CORDER_TERMINATION_SEQUENCE = 8;
const int CORDER_USE_TASK = 9;
const int CORDER_COUNT = 10;
extern TagDict<10> CORDER_tagDict;

const int CTASK_CANCELLATION_SEQUENCE = 0;
const int CTASK_CLASS_TYPE = 1;
const int CTASK_FLAG = 2;
const int CTASK_IDENTIFY_TARGET_EACH_CYCLE = 3;
const int CTASK_INITIALISATION_SEQUENCE = 4;
const int CTASK_IN_TASK_CATEGORY = 5;
const int CTASK_MOVEMENT_COMPLETED_SEQUENCE = 6;
const int CTASK_MOVEMENT_STARTED_SEQUENCE = 7;
const int CTASK_PLAY_ANIMATION = 8;
const int CTASK_PLAY_ANIMATION_ONCE = 9;
const int CTASK_PROXIMITY_DISSATISFIED_SEQUENCE = 10;
const int CTASK_PROXIMITY_REQUIREMENT = 11;
const int CTASK_PROXIMITY_SATISFIED_SEQUENCE = 12;
const int CTASK_REJECT_TARGET_IF_IT_IS_TERMINATED = 13;
const int CTASK_RESUMPTION_SEQUENCE = 14;
const int CTASK_START_SEQUENCE = 15;
const int CTASK_SUSPENSION_SEQUENCE = 16;
const int CTASK_SYNCH_ANIMATION_TO_FRACTION = 17;
const int CTASK_TASK_TARGET = 18;
const int CTASK_TERMINATE_ENTIRE_ORDER_IF_NO_TARGET = 19;
const int CTASK_TERMINATION_SEQUENCE = 20;
const int CTASK_TRIGGER = 21;
const int CTASK_USE_PREVIOUS_TASK_TARGET = 22;
const int CTASK_COUNT = 23;
extern TagDict<23> CTASK_tagDict;

const int ORDERASSIGNMODE_DO_FIRST = 0;
const int ORDERASSIGNMODE_DO_LAST = 1;
const int ORDERASSIGNMODE_FORGET_EVERYTHING_ELSE = 2;
const int ORDERASSIGNMODE_COUNT = 3;
extern TagDict<3> ORDERASSIGNMODE_tagDict;

const int CORDERASSIGN_ORDER_ASSIGNMENT_MODE = 0;
const int CORDERASSIGN_ORDER_TARGET = 1;
const int CORDERASSIGN_ORDER_TO_ASSIGN = 2;
const int CORDERASSIGN_COUNT = 3;
extern TagDict<3> CORDERASSIGN_tagDict;

const int TASKTRIGGER_ANIMATION_LOOP = 0;
const int TASKTRIGGER_ATTACHMENT_POINT = 1;
const int TASKTRIGGER_COLLISION = 2;
const int TASKTRIGGER_STRUCK_FLOOR = 3;
const int TASKTRIGGER_TIMER = 4;
const int TASKTRIGGER_UNINTERRUPTIBLE_ANIMATION_LOOP = 5;
const int TASKTRIGGER_COUNT = 6;
extern TagDict<6> TASKTRIGGER_tagDict;

const int DISTCALCMODE_3D = 0;
const int DISTCALCMODE_HORIZONTAL = 1;
const int DISTCALCMODE_VERTICAL = 2;
const int DISTCALCMODE_COUNT = 3;
extern TagDict<3> DISTCALCMODE_tagDict;

const int OBJFINDCOND_ALLIED_UNITS = 0;
const int OBJFINDCOND_BUILDINGS_ONLY = 1;
const int OBJFINDCOND_CHARACTERS_AND_BUILDINGS_ONLY = 2;
const int OBJFINDCOND_CHARACTERS_ONLY = 3;
const int OBJFINDCOND_ENEMY_UNITS = 4;
const int OBJFINDCOND_ORIGINAL_ALLIED_UNITS = 5;
const int OBJFINDCOND_ORIGINAL_ENEMY_UNITS = 6;
const int OBJFINDCOND_ORIGINAL_SAME_PLAYER_UNITS = 7;
const int OBJFINDCOND_SAME_PLAYER_UNITS = 8;
const int OBJFINDCOND_COUNT = 9;
extern TagDict<9> OBJFINDCOND_tagDict;

const int PACKAGE_ITEM_MOD_INCREASE = 0;
const int PACKAGE_ITEM_MOD_REDUCE = 1;
const int PACKAGE_ITEM_MOD_REPLACE = 2;
const int PACKAGE_ITEM_MOD_COUNT = 3;
extern TagDict<3> PACKAGE_ITEM_MOD_tagDict;

const int GTW_BUTTON_WINDOW_ACTION_CLOSE_WINDOW = 0;
const int GTW_BUTTON_WINDOW_ACTION_MOVE_FIRST_PAGE = 1;
const int GTW_BUTTON_WINDOW_ACTION_MOVE_NEXT_PAGE = 2;
const int GTW_BUTTON_WINDOW_ACTION_MOVE_PREVIOUS_PAGE = 3;
const int GTW_BUTTON_WINDOW_ACTION_COUNT = 4;
extern TagDict<4> GTW_BUTTON_WINDOW_ACTION_tagDict;

const int SREQSTATE_ACTIVE = 0;
const int SREQSTATE_BLOCKED = 1;
const int SREQSTATE_COMPLETE = 2;
const int SREQSTATE_STALLED = 3;
const int SREQSTATE_COUNT = 4;
extern TagDict<4> SREQSTATE_tagDict;

const int SREQDETAILEDSTATE_CANT_AFFORD = 0;
const int SREQDETAILEDSTATE_FOUNDATION_CREATION_FAILED = 1;
const int SREQDETAILEDSTATE_IMPOSSIBLE_CONDITION_FAILED = 2;
const int SREQDETAILEDSTATE_NO_SPAWN_LOCATIONS_IDLE = 3;
const int SREQDETAILEDSTATE_NO_SPAWN_LOCATION_FOUND = 4;
const int SREQDETAILEDSTATE_NO_UPGRADE_LOCATIONS_IDLE = 5;
const int SREQDETAILEDSTATE_NO_UPGRADE_LOCATION_FOUND = 6;
const int SREQDETAILEDSTATE_NO_VALID_POSITION = 7;
const int SREQDETAILEDSTATE_OK = 8;
const int SREQDETAILEDSTATE_RESOURCE_REQUIRED_RESERVED = 9;
const int SREQDETAILEDSTATE_SPAWN_ORDER_ASSIGNMENT_FAILED = 10;
const int SREQDETAILEDSTATE_UNPROCESSED = 11;
const int SREQDETAILEDSTATE_UPGRADE_ORDER_ASSIGNMENT_FAILED = 12;
const int SREQDETAILEDSTATE_WAIT_CONDITION_FAILED = 13;
const int SREQDETAILEDSTATE_COUNT = 14;
extern TagDict<14> SREQDETAILEDSTATE_tagDict;

const int SREQUIREMENTCLASS_BUILDING_REQUIREMENT = 0;
const int SREQUIREMENTCLASS_CHARACTER_REQUIREMENT = 1;
const int SREQUIREMENTCLASS_UPGRADE_REQUIREMENT = 2;
const int SREQUIREMENTCLASS_COUNT = 3;
extern TagDict<3> SREQUIREMENTCLASS_tagDict;

const int BUILDINGTYPE_CIVIC = 0;
const int BUILDINGTYPE_CIVIC_CENTRE = 1;
const int BUILDINGTYPE_GATEHOUSE = 2;
const int BUILDINGTYPE_RURAL = 3;
const int BUILDINGTYPE_RURAL_CENTRE = 4;
const int BUILDINGTYPE_TOWER = 5;
const int BUILDINGTYPE_TOWER_CORNER_IN = 6;
const int BUILDINGTYPE_TOWER_CORNER_OUT = 7;
const int BUILDINGTYPE_WALL = 8;
const int BUILDINGTYPE_WALL_CORNER_IN = 9;
const int BUILDINGTYPE_WALL_CORNER_OUT = 10;
const int BUILDINGTYPE_WALL_CROSSROADS = 11;
const int BUILDINGTYPE_COUNT = 12;
extern TagDict<12> BUILDINGTYPE_tagDict;

const int PDITEM_ACTUAL_SIGHT_RANGE = 0;
const int PDITEM_FOOD = 1;
const int PDITEM_GOLD = 2;
const int PDITEM_HIT_POINTS = 3;
const int PDITEM_HIT_POINTS_OF_OBJECT_BEING_SPAWNED = 4;
const int PDITEM_HIT_POINT_CAPACITY = 5;
const int PDITEM_HIT_POINT_CAPACITY_OF_OBJECT_BEING_SPAWNED = 6;
const int PDITEM_NUMBER_OF_FARMERS = 7;
const int PDITEM_STONE = 8;
const int PDITEM_WOOD = 9;
const int PDITEM_COUNT = 10;
extern TagDict<10> PDITEM_tagDict;

const int MAPTEXDIR_EAST = 0;
const int MAPTEXDIR_NORTH = 1;
const int MAPTEXDIR_SOUTH = 2;
const int MAPTEXDIR_WEST = 3;
const int MAPTEXDIR_COUNT = 4;
extern TagDict<4> MAPTEXDIR_tagDict;

const int GS_SIDE_CRITICAL_FOR_CLIENT = 0;
const int GS_SIDE_NORMAL = 1;
const int GS_SIDE_SERVER_ONLY = 2;
const int GS_SIDE_COUNT = 3;
extern TagDict<3> GS_SIDE_tagDict;

} // end namespace
