import { test, expect } from '@playwright/test';

const V2 = '/api/v2';

// 192.0.2.1 is TEST-NET (RFC 5737), safe for dummy proxy tests
const TEST_PROXY_IP = '192.0.2.1';

test.describe('proxies', () => {

  test('GET /proxies', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'FULL' });
    const res = await request.get(`${V2}/proxies`);
    expect(res.status()).toBe(200);
    expect(await res.json()).toEqual([]);
  });

  test('POST /proxies — set full list then restore', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    const setRes = await request.post(`${V2}/proxies`, {
      data: [TEST_PROXY_IP],
    });
    expect(setRes.status()).toBe(200);

    const restoreRes = await request.post(`${V2}/proxies`, { data: [] });
    expect(restoreRes.status()).toBe(200);
  });

  test('DELETE /proxies — delete all after setup', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    await request.post(`${V2}/proxies`, { data: [TEST_PROXY_IP] });

    const res = await request.delete(`${V2}/proxies`);
    expect(res.status()).toBe(200);
  });

  test('POST /proxies/:ProxyIp then DELETE — add then remove', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    const add = await request.post(`${V2}/proxies/${TEST_PROXY_IP}`);
    expect(add.status()).toBe(200);

    const del = await request.delete(`${V2}/proxies/${TEST_PROXY_IP}`);
    expect(del.status()).toBe(200);
  });

  test('GET /system/proxies', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/system/proxies`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(Array.isArray(body)).toBe(true);
  });

  test('POST /system/proxies — set full list then restore', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    const setRes = await request.post(`${V2}/system/proxies`, {
      data: [TEST_PROXY_IP],
    });
    expect(setRes.status()).toBe(200);

    const restoreRes = await request.post(`${V2}/system/proxies`, { data: [] });
    expect(restoreRes.status()).toBe(200);
  });

});
