import { test, expect } from '@playwright/test';
import { createTestPlaylist, deleteTestPlaylist, startTestPlaylist, PLAYLIST_NAME, V2 } from '../../fixtures/playback';

test.describe('playlists', () => {

  test('GET /playlists', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/playlists`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(Array.isArray(body)).toBe(true);
  });

  test('POST /playlists — insert then clean up', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    const res = await request.post(`${V2}/playlists`, {
      data: {
        name: PLAYLIST_NAME,
        mainPlaylist: [{ type: 'pause', duration: 60 }],
        playlistInfo: { loop: false, description: 'CI test playlist' },
      },
    });
    expect(res.status()).toBe(200);
    await request.delete(`${V2}/playlist/${PLAYLIST_NAME}`);
  });

  test('GET /playlists/playable', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/playlists/playable`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(Array.isArray(body)).toBe(true);
  });

  test('GET /playlists/validate', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/playlists/validate`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
  });

  test('POST /playlists/stop', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: a running playlist; stopping with nothing playing should still return 200
    await createTestPlaylist(request);
    await startTestPlaylist(request);
    const res = await request.post(`${V2}/playlists/stop`);
    expect(res.status()).toBe(200);
    await request.delete(`${V2}/playlist/${PLAYLIST_NAME}`);
  });

  test('POST /playlists/pause', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: a running playlist to pause
    await createTestPlaylist(request);
    await startTestPlaylist(request);
    const res = await request.post(`${V2}/playlists/pause`);
    expect(res.status()).toBe(200);
    await deleteTestPlaylist(request);
  });

  test('POST /playlists/resume', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: a paused playlist to resume
    await createTestPlaylist(request);
    await startTestPlaylist(request);
    await request.post(`${V2}/playlists/pause`);
    const res = await request.post(`${V2}/playlists/resume`);
    expect(res.status()).toBe(200);
    await deleteTestPlaylist(request);
  });

  test('POST /playlists/stopgracefully', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    await createTestPlaylist(request);
    await startTestPlaylist(request);
    const res = await request.post(`${V2}/playlists/stopgracefully`);
    expect(res.status()).toBe(200);
    await request.delete(`${V2}/playlist/${PLAYLIST_NAME}`);
  });

  test('POST /playlists/stopgracefullyafterloop', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    await createTestPlaylist(request);
    await startTestPlaylist(request);
    const res = await request.post(`${V2}/playlists/stopgracefullyafterloop`);
    expect(res.status()).toBe(200);
    await deleteTestPlaylist(request);
  });

});
