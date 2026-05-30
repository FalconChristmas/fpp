import { test, expect } from '@playwright/test';

const V2 = '/api/v2';

test.describe('pipewire', () => {

  test('GET /pipewire/audio/groups', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.get(`${V2}/pipewire/audio/groups`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('POST /pipewire/audio/groups', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.post(`${V2}/pipewire/audio/groups`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('POST /pipewire/audio/groups/apply', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.post(`${V2}/pipewire/audio/groups/apply`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('GET /pipewire/audio/sinks', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.get(`${V2}/pipewire/audio/sinks`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('GET /pipewire/audio/cards', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.get(`${V2}/pipewire/audio/cards`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('GET /pipewire/audio/sources', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.get(`${V2}/pipewire/audio/sources`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('GET /pipewire/audio/input-groups', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.get(`${V2}/pipewire/audio/input-groups`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('POST /pipewire/audio/input-groups', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.post(`${V2}/pipewire/audio/input-groups`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('POST /pipewire/audio/input-groups/apply', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.post(`${V2}/pipewire/audio/input-groups/apply`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('POST /pipewire/audio/input-groups/volume', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.post(`${V2}/pipewire/audio/input-groups/volume`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('POST /pipewire/audio/input-groups/effects', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.post(`${V2}/pipewire/audio/input-groups/effects`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('POST /pipewire/audio/input-groups/eq/update', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.post(`${V2}/pipewire/audio/input-groups/eq/update`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('GET /pipewire/audio/routing', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.get(`${V2}/pipewire/audio/routing`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('POST /pipewire/audio/routing', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.post(`${V2}/pipewire/audio/routing`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('POST /pipewire/audio/routing/volume', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.post(`${V2}/pipewire/audio/routing/volume`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('GET /pipewire/audio/routing/presets', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.get(`${V2}/pipewire/audio/routing/presets`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('GET /pipewire/audio/routing/presets/names', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.get(`${V2}/pipewire/audio/routing/presets/names`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('POST /pipewire/audio/routing/presets', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.post(`${V2}/pipewire/audio/routing/presets`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('POST /pipewire/audio/routing/presets/load', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.post(`${V2}/pipewire/audio/routing/presets/load`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('POST /pipewire/audio/routing/presets/live-apply', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.post(`${V2}/pipewire/audio/routing/presets/live-apply`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('DELETE /pipewire/audio/routing/presets/:name', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    // Requires: a preset with the given name to exist
    test.skip(true, 'Requires a pre-existing named routing preset');
  });

  test('POST /pipewire/audio/stream/volume', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.post(`${V2}/pipewire/audio/stream/volume`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('GET /pipewire/audio/stream/status', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.get(`${V2}/pipewire/audio/stream/status`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('POST /pipewire/audio/group/volume', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.post(`${V2}/pipewire/audio/group/volume`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('POST /pipewire/audio/eq/update', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.post(`${V2}/pipewire/audio/eq/update`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('POST /pipewire/audio/delay/update', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.post(`${V2}/pipewire/audio/delay/update`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('POST /pipewire/audio/sync/start', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.post(`${V2}/pipewire/audio/sync/start`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('POST /pipewire/audio/sync/stop', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.post(`${V2}/pipewire/audio/sync/stop`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('GET /pipewire/video/groups', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.get(`${V2}/pipewire/video/groups`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('POST /pipewire/video/groups', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.post(`${V2}/pipewire/video/groups`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('POST /pipewire/video/groups/apply', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.post(`${V2}/pipewire/video/groups/apply`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('POST /pipewire/simple/apply', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.post(`${V2}/pipewire/simple/apply`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('GET /pipewire/video/connectors', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.get(`${V2}/pipewire/video/connectors`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('GET /pipewire/video/routing', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.get(`${V2}/pipewire/video/routing`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('POST /pipewire/video/routing', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.post(`${V2}/pipewire/video/routing`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('GET /pipewire/video/input-sources', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.get(`${V2}/pipewire/video/input-sources`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('POST /pipewire/video/input-sources', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.post(`${V2}/pipewire/video/input-sources`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('POST /pipewire/video/input-sources/apply', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.post(`${V2}/pipewire/video/input-sources/apply`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('GET /pipewire/video/input-sources/v4l2-devices', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.get(`${V2}/pipewire/video/input-sources/v4l2-devices`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('GET /pipewire/aes67/instances', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.get(`${V2}/pipewire/aes67/instances`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('POST /pipewire/aes67/instances', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.post(`${V2}/pipewire/aes67/instances`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('POST /pipewire/aes67/apply', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.post(`${V2}/pipewire/aes67/apply`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('GET /pipewire/aes67/status', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.get(`${V2}/pipewire/aes67/status`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('GET /pipewire/aes67/interfaces', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.get(`${V2}/pipewire/aes67/interfaces`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('GET /pipewire/graph', { tag: ['@hardware:pipewire'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.get(`${V2}/pipewire/graph`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

});
