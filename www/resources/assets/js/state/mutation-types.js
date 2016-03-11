/*
 * Device Mutations
 */
export const REBOOT_DEVICE   = 'REBOOT_DEVICE';
export const SHUTDOWN_DEVICE = 'SHUTDOWN_DEVICE';
export const DEVICE_BOOTED = 'DEVICE_BOOTED';
export const REQUIRE_REBOOT = 'REQUIRE_REBOOT';

/*
 * FPPD Mutations
 */
export const START_FPPD      = 'START_FPPD';
export const STOP_FPPD       = 'STOP_FPPD';
export const RESTART_FPPD    = 'RESTART_FPPD';
export const REQUIRE_RESTART = 'REQUIRE_RESTART';
export const UPDATE_MODE = 'UPDATE_MODE';

/**
 * Status Mutations
 */
export const STATUS_REQUEST = 'STATUS_REQUEST';
export const STATUS_UPDATE = 'STATUS_UPDATE';

/**
 * Player
 */
export const UPDATE_VOLUME = 'UPDATE_VOLUME';
export const LOAD_PLAYLIST = 'LOAD_PLAYLIST';
export const LOAD_PLAYLISTS = 'LOAD_PLAYLISTS';

/**
 * Outputs
 */
export const TOGGLE_OUTPUTS = 'TOGGLE_OUTPUTS';
export const RECEIVE_OUTPUTS = 'RECEIVE_OUTPUTS';
export const ADD_OUTPUT = 'ADD_OUTPUT';
export const REMOVE_OUTPUT = 'REMOVE_OUTPUT';
export const UPDATE_OUTPUT = 'UPDATE_OUTPUT';