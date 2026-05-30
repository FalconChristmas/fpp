import { test, expect } from '@playwright/test';

const V2 = '/api/v2';

test.describe('network', () => {

  test('GET /network/dns', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/network/dns`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('POST /network/dns', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Read existing DNS config and write it back unchanged to avoid disruption
    const getRes = await request.get(`${V2}/network/dns`);
    expect(getRes.status()).toBe(200);
    const original = await getRes.json();

    const saveRes = await request.post(`${V2}/network/dns`, { data: original });
    expect(saveRes.status()).toBe(200);
  });

  test('GET /network/gateway', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/network/gateway`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('POST /network/gateway', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Read existing gateway config and write it back unchanged
    const getRes = await request.get(`${V2}/network/gateway`);
    expect(getRes.status()).toBe(200);
    const original = await getRes.json();

    const saveRes = await request.post(`${V2}/network/gateway`, { data: original });
    expect(saveRes.status()).toBe(200);
  });

  test('GET /network/interface', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/network/interface`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(Array.isArray(body)).toBe(true);
  });

  test('GET /network/interface/:interface', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    // `lo` (loopback) is guaranteed to exist on all Linux and macOS systems
    const res = await request.get(`${V2}/network/interface/lo`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('POST /network/interface/add/:interface', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: valid interface name and config; skip to avoid disrupting network
    test.skip(true, 'Requires valid interface name — risk of disrupting network config');
  });

  test('POST /network/interface/:interface', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Read existing lo config and write it back unchanged
    const getRes = await request.get(`${V2}/network/interface/lo`);
    expect(getRes.status()).toBe(200);
    const original = await getRes.json();

    const saveRes = await request.post(`${V2}/network/interface/lo`, { data: original });
    expect(saveRes.status()).toBe(200);
  });

  test('POST /network/interface/:interface/apply', { tag: ['@destructive'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'DESTRUCTIVE' });
    // Applies network configuration live — may briefly interrupt connectivity
    const res = await request.post(`${V2}/network/interface/lo/apply`);
    expect([200, 202]).toContain(res.status());
  });

  test('DELETE /network/persistentNames', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    const res = await request.delete(`${V2}/network/persistentNames`);
    expect(res.status()).toBe(200);
  });

  test('POST /network/persistentNames', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    const res = await request.post(`${V2}/network/persistentNames`);
    expect(res.status()).toBe(200);
  });

  test('GET /network/wifi/scan/:interface', { tag: ['@hardware:wifi'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    // Requires: WiFi interface name — common values are wlan0 or wlan1
    test.skip(true, 'Requires WiFi interface name which is device-specific');
  });

  test('GET /network/wifi/strength', { tag: ['@hardware:wifi'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.get(`${V2}/network/wifi/strength`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

});
