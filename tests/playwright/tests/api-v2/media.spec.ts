import { test, expect } from '@playwright/test';

const V2 = '/api/v2';

test.describe('media', () => {

  test('GET /media', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/media`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(Array.isArray(body)).toBe(true);
  });

  test('GET /media/:MediaName/duration', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: a media file to exist on the device with a known name
    test.skip(true, 'Requires a pre-existing media file on the device');
  });

  test('GET /media/:MediaName/meta', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: a media file to exist on the device with a known name
    test.skip(true, 'Requires a pre-existing media file on the device');
  });

});
