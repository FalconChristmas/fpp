import { test, expect } from '@playwright/test';

const V2 = '/api/v2';

test.describe('cape', () => {

  test('GET /cape', { tag: ['@hardware:cape'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.get(`${V2}/cape`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('POST /cape/eeprom/voucher', { tag: ['@hardware:cape'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.post(`${V2}/cape/eeprom/voucher`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('POST /cape/eeprom/sign/:key/:order', { tag: ['@hardware:cape'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    // Requires: valid key and order values from a real cape provisioning workflow
    test.skip(true, 'Requires valid cape signing key and order');
  });

  test('GET /cape/eeprom/signingData/:key/:order', { tag: ['@hardware:cape'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    // Requires: valid key and order values from a real cape provisioning workflow
    test.skip(true, 'Requires valid cape signing key and order');
  });

  test('GET /cape/eeprom/signingFile/:key/:order', { tag: ['@hardware:cape'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    // Requires: valid key and order values from a real cape provisioning workflow
    test.skip(true, 'Requires valid cape signing key and order');
  });

  test('POST /cape/eeprom/signingData', { tag: ['@hardware:cape'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    const res = await request.post(`${V2}/cape/eeprom/signingData`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('GET /cape/options', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/cape/options`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(body).not.toBeNull();
    expect(typeof body).toBe('object');
  });

  test('GET /cape/strings', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/cape/strings`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(Array.isArray(body)).toBe(true);
  });

  test('GET /cape/panel', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/cape/panel`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(Array.isArray(body)).toBe(true);
  });

  test('GET /cape/strings/:key', { tag: ['@hardware:cape'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    // Requires: cape with string outputs configured; key must be a valid string config key
    test.skip(true, 'Requires physical cape with string outputs');
  });

  test('GET /cape/panel/:key', { tag: ['@hardware:cape'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'HARDWARE' });
    // Requires: cape with panel outputs configured; key must be a valid panel config key
    test.skip(true, 'Requires physical cape with panel outputs');
  });

});
