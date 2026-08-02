import { test as base, expect } from '@playwright/test';

export const PLAYLIST_NAME = 'ci-test-playlist';
export const V2 = '/api/v2';

export async function createTestPlaylist(request: import('@playwright/test').APIRequestContext) {
  await request.post(`${V2}/playlist/${PLAYLIST_NAME}`, {
    data: {
      name: PLAYLIST_NAME,
      mainPlaylist: [{ type: 'pause', duration: 60 }],
      playlistInfo: { loop: false, description: 'CI test playlist' },
    },
  });
}

export async function startTestPlaylist(request: import('@playwright/test').APIRequestContext) {
  await request.post(`${V2}/playlist/${PLAYLIST_NAME}/start/0`);
}

export async function deleteTestPlaylist(request: import('@playwright/test').APIRequestContext) {
  await request.post(`${V2}/playlists/stop`);
  await request.delete(`${V2}/playlist/${PLAYLIST_NAME}`);
}

export { test as base, expect } from '@playwright/test';
