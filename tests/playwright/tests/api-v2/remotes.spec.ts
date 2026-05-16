import { test, expect } from '@playwright/test';

const V2 = '/api/v2';

test.describe('remotes', () => {

  test('GET /remotes', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/remotes`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(Array.isArray(body)).toBe(true);
  });

  test('GET /proxy/:Ip/:urlPart', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: a reachable remote FPP instance at a known IP address
    test.skip(true, 'Requires a reachable remote FPP instance at a known IP');
  });

  test('POST /remoteAction', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: a reachable remote FPP instance; body: {"ip":"...","action":"..."}
    test.skip(true, 'Requires a reachable remote FPP instance at a known IP');
  });

});
