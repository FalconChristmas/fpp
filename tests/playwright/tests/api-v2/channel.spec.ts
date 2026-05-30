import { test, expect } from '@playwright/test';

const V2 = '/api/v2';

test.describe('channel', () => {

  test('GET /channel/input/stats', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/channel/input/stats`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('DELETE /channel/input/stats — resets stats', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    const res = await request.delete(`${V2}/channel/input/stats`);
    expect(res.status()).toBe(200);
  });

  test('GET /channel/output/processors', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/channel/output/processors`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(Array.isArray(body)).toBe(true);
  });

  test('POST /channel/output/processors', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Read current processors first so we can restore them
    const getRes = await request.get(`${V2}/channel/output/processors`);
    expect(getRes.status()).toBe(200);
    const original = await getRes.json();

    const saveRes = await request.post(`${V2}/channel/output/processors`, { data: original });
    expect(saveRes.status()).toBe(200);
  });

  test('GET /channel/output/:file', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/channel/output/channeloutputs`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('POST /channel/output/:file', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Read current config and write it back unchanged
    const getRes = await request.get(`${V2}/channel/output/channeloutputs`);
    expect(getRes.status()).toBe(200);
    const original = await getRes.json();

    const saveRes = await request.post(`${V2}/channel/output/channeloutputs`, { data: original });
    expect(saveRes.status()).toBe(200);
  });

});
