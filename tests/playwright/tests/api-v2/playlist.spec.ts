import { test, expect } from '@playwright/test';
import { createTestPlaylist, deleteTestPlaylist, startTestPlaylist, PLAYLIST_NAME, V2 } from '../../fixtures/playback';

test.describe('playlist', () => {

  test('GET /playlist/:PlaylistName', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    await createTestPlaylist(request);

    const res = await request.get(`${V2}/playlist/${PLAYLIST_NAME}`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).toHaveProperty('name', PLAYLIST_NAME);

    await deleteTestPlaylist(request);
  });

  test('POST /playlist/:PlaylistName — upsert self-contained', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    const res = await request.post(`${V2}/playlist/${PLAYLIST_NAME}`, {
      data: {
        name: PLAYLIST_NAME,
        mainPlaylist: [{ type: 'pause', duration: 60 }],
        playlistInfo: { loop: false, description: 'CI test playlist' },
      },
    });
    expect(res.status()).toBe(200);

    await request.delete(`${V2}/playlist/${PLAYLIST_NAME}`);
  });

  test('DELETE /playlist/:PlaylistName', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    await createTestPlaylist(request);

    const res = await request.delete(`${V2}/playlist/${PLAYLIST_NAME}`);
    expect(res.status()).toBe(200);
  });

  test('POST /playlist/:PlaylistName/start', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    await createTestPlaylist(request);

    const res = await request.post(`${V2}/playlist/${PLAYLIST_NAME}/start`);
    expect(res.status()).toBe(200);

    await deleteTestPlaylist(request);
  });

  test('POST /playlist/:PlaylistName/start/:Repeat', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    await createTestPlaylist(request);

    const res = await request.post(`${V2}/playlist/${PLAYLIST_NAME}/start/0`);
    expect(res.status()).toBe(200);

    await deleteTestPlaylist(request);
  });

  test('POST /playlist/:PlaylistName/start/:Repeat/:ScheduleProtected', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    await createTestPlaylist(request);

    const res = await request.post(`${V2}/playlist/${PLAYLIST_NAME}/start/0/0`);
    expect(res.status()).toBe(200);

    await deleteTestPlaylist(request);
  });

  test('POST /playlist/:PlaylistName/:SectionName/item', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    await createTestPlaylist(request);

    const res = await request.post(`${V2}/playlist/${PLAYLIST_NAME}/mainPlaylist/item`, {
      data: { type: 'pause', duration: 5 },
    });
    expect(res.status()).toBe(200);

    await deleteTestPlaylist(request);
  });

});
