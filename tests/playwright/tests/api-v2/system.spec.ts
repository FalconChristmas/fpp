import { test, expect } from '@playwright/test';

const V2 = '/api/v2';

test.describe('system', () => {

  test('GET /system/info', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/system/info`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).toHaveProperty('Version');
    expect(body).toHaveProperty('Hostname');
    expect(typeof body.Version).toBe('string');
    expect(typeof body.Hostname).toBe('string');
  });

  test('GET /system/status', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/system/status`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).toHaveProperty('status');
    expect(typeof body.status).toBe('string');
  });

  test('GET /system/updateStatus', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/system/updateStatus`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('GET /system/releaseNotes/:version', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/system/releaseNotes/current`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
  });

  test('GET /system/packages', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/system/packages`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(Array.isArray(body)).toBe(true);
  });

  test('GET /system/packages/info/:packageName', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/system/packages/info/fpp`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('POST /system/fppd/skipBootDelay', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    const res = await request.post(`${V2}/system/fppd/skipBootDelay`);
    expect(res.status()).toBe(200);
  });

  test('GET /system/volume', { tag: ['@hardware:audio'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.get(`${V2}/system/volume`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('POST /system/volume', { tag: ['@hardware:audio'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.post(`${V2}/system/volume`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('POST /system/fppd/restart', { tag: ['@destructive'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'DESTRUCTIVE' });
    // Kills and restarts fppd — all in-flight playback stops
    const res = await request.post(`${V2}/system/fppd/restart`);
    expect([200, 202]).toContain(res.status());
  });

  test('POST /system/fppd/start', { tag: ['@destructive'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'DESTRUCTIVE' });
    // Starts fppd — may return an error status if fppd is already running
    const res = await request.post(`${V2}/system/fppd/start`);
    expect([200, 202]).toContain(res.status());
  });

  test('POST /system/fppd/stop', { tag: ['@destructive'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'DESTRUCTIVE' });
    // Kills fppd — all playback stops and the daemon goes offline
    const res = await request.post(`${V2}/system/fppd/stop`);
    expect([200, 202]).toContain(res.status());
  });

  test('POST /system/reboot', { tag: ['@destructive'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'DESTRUCTIVE' });
    // Reboots the OS — SSH and all services become unavailable
    const res = await request.post(`${V2}/system/reboot`);
    expect([200, 202]).toContain(res.status());
  });

  test('POST /system/shutdown', { tag: ['@destructive'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'DESTRUCTIVE' });
    // Shuts down the OS — device becomes completely unreachable
    const res = await request.post(`${V2}/system/shutdown`);
    expect([200, 202]).toContain(res.status());
  });

});
